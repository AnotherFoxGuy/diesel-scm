#include "Workspace.h"
#include <QCoreApplication>
#include "Utils.h"

//-----------------------------------------------------------------------------
Workspace::Workspace()
{
}
//-----------------------------------------------------------------------------
Workspace::~Workspace()
{
	clearState();
	remotes.clear();
}

//------------------------------------------------------------------------------
void Workspace::clearState()
{
	// Dispose RepoFiles
	foreach(WorkspaceFile *r, getFiles())
		delete r;

	getFiles().clear();
	getPaths().clear();
	stashMap.clear();
	branchList.clear();
	tags.clear();
	isIntegrated = false;
}

//------------------------------------------------------------------------------
void Workspace::storeWorkspace(QSettings &store)
{
	QString workspace = fossil().getCurrentWorkspace();
	if(workspace.isEmpty())
		return;

	store.beginGroup("Remotes");
	QString workspace_hash = HashString(QDir::toNativeSeparators(workspace));

	store.beginWriteArray(workspace_hash);
	int index = 0;
	for(remote_map_t::iterator it=remotes.begin(); it!=remotes.end(); ++it, ++index)
	{
		store.setArrayIndex(index);
		store.setValue("Name", it->name);
		QUrl url = it->url;
		url.setPassword("");
		store.setValue("Url", url);
		if(it->isDefault)
			store.setValue("Default", it->isDefault);
		else
			store.remove("Default");
	}
	store.endArray();
	store.endGroup();

}

//------------------------------------------------------------------------------
bool Workspace::switchWorkspace(const QString& workspace, QSettings &store)
{
	// Save Remotes
	storeWorkspace(store);
	clearState();
	remotes.clear();

	fossil().setCurrentWorkspace("");
	if(workspace.isEmpty())
		return true;

	QString new_workspace = QFileInfo(workspace).absoluteFilePath();

	if(!QDir::setCurrent(new_workspace))
		return false;

	fossil().setCurrentWorkspace(new_workspace);

	// Load Remotes
	QString workspace_hash = HashString(QDir::toNativeSeparators(new_workspace));

	QString gr = store.group();

	store.beginGroup("Remotes");
	gr = store.group();
	int num_remotes = store.beginReadArray(workspace_hash);
	for(int i=0; i<num_remotes; ++i)
	{
		store.setArrayIndex(i);

		QString name = store.value("Name").toString();
		QUrl url = store.value("Url").toUrl();
		bool def = store.value("Default", false).toBool();
		addRemote(url, name);
		if(def)
			setRemoteDefault(url);
	}
	store.endArray();
	store.endGroup();

	// Add the default url from fossil
	QUrl default_remote;
	if(fossil().getRemoteUrl(default_remote) && default_remote.isValid() && !default_remote.isEmpty())
	{
		addRemote(default_remote, default_remote.toDisplayString());
		setRemoteDefault(default_remote);
	}

	return true;
}

//------------------------------------------------------------------------------
bool Workspace::scanDirectory(QFileInfoList &entries, const QString& dirPath, const QString &baseDir, const QString ignoreSpec, const bool &abort, UICallback &uiCallback)
{
	QDir dir(dirPath);

	uiCallback.updateProcess(dirPath);

	QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
	for (int i=0; i<list.count(); ++i)
	{
		if(abort)
			return false;

		QFileInfo info = list[i];
		QString filepath = info.filePath();
		QString rel_path = filepath;
		rel_path.remove(baseDir+PATH_SEPARATOR);

		// Skip ignored files
		if(!ignoreSpec.isEmpty() && QDir::match(ignoreSpec, rel_path))
			continue;

		if (info.isDir())
		{
			if(!scanDirectory(entries, filepath, baseDir, ignoreSpec, abort, uiCallback))
				return false;
		}
		else
			entries.push_back(info);
	}
	return true;
}

//------------------------------------------------------------------------------
void Workspace::scanWorkspace(bool scanLocal, bool scanIgnored, bool scanModified, bool scanUnchanged, const QString &ignoreGlob, UICallback &uiCallback, bool &operationAborted)
{
	// Scan all workspace files
	QFileInfoList all_files;
	QString wkdir = fossil().getCurrentWorkspace();

	if(wkdir.isEmpty())
		return;

	// Retrieve the status of files tracked by fossil
	QStringList res;
	if(!fossil().listFiles(res))
		return;

	bool scan_files = scanLocal;

	clearState();

	operationAborted = false;

	uiCallback.beginProcess("");
	if(scan_files)
	{
		QCoreApplication::processEvents();

		QString ignore;
		// If we should not be showing ignored files, fill in the ignored spec
		if(!scanIgnored)
		{
			// QDir expects multiple specs being separated by a semicolon
			ignore = ignoreGlob;
			ignore.replace(',',';');
		}

		if(!scanDirectory(all_files, wkdir, wkdir, ignore, operationAborted, uiCallback))
			goto _done;

		for(QFileInfoList::iterator it=all_files.begin(); it!=all_files.end(); ++it)
		{
			QString filename = it->fileName();
			QString fullpath = it->absoluteFilePath();

			// Skip fossil files
			if(filename == FOSSIL_CHECKOUT1 || filename == FOSSIL_CHECKOUT2 || (!fossil().getRepositoryFile().isEmpty() && QFileInfo(fullpath) == QFileInfo(fossil().getRepositoryFile())))
				continue;

			WorkspaceFile *rf = new WorkspaceFile(*it, WorkspaceFile::TYPE_UNKNOWN, wkdir);
			getFiles().insert(rf->getFilePath(), rf);
			getPaths().insert(rf->getPath());
		}
	}
	uiCallback.endProcess();

	uiCallback.beginProcess(QObject::tr("Updating..."));

	// Update Files and Directories

	for(QStringList::iterator line_it=res.begin(); line_it!=res.end(); ++line_it)
	{
		QString line = (*line_it).trimmed();
		if(line.length()==0)
			continue;

		int space_index = line.indexOf(' ');
		if(space_index==-1)
			continue;

		QString status_text = line.left(space_index);
		QString fname = line.right(line.length() - space_index).trimmed();
		WorkspaceFile::Type type = WorkspaceFile::TYPE_UNKNOWN;

		// Generate a RepoFile for all non-existant fossil files
		// or for all files if we skipped scanning the workspace
		bool add_missing = !scan_files;

		if(status_text=="EDITED")
			type = WorkspaceFile::TYPE_EDITTED;
		else if(status_text=="ADDED")
			type = WorkspaceFile::TYPE_ADDED;
		else if(status_text=="DELETED")
		{
			type = WorkspaceFile::TYPE_DELETED;
			add_missing = true;
		}
		else if(status_text=="MISSING")
		{
			type = WorkspaceFile::TYPE_MISSING;
			add_missing = true;
		}
		else if(status_text=="RENAMED")
			type = WorkspaceFile::TYPE_RENAMED;
		else if(status_text=="UNCHANGED")
			type = WorkspaceFile::TYPE_UNCHANGED;
		else if(status_text=="CONFLICT")
			type = WorkspaceFile::TYPE_CONFLICTED;
		else if(status_text=="UPDATED_BY_MERGE" || status_text=="ADDED_BY_MERGE" || status_text=="ADDED_BY_INTEGRATE" || status_text=="UPDATED_BY_INTEGRATE")
			type = WorkspaceFile::TYPE_MERGED;

		// Filter unwanted file types
		if( ((type & WorkspaceFile::TYPE_MODIFIED) && !scanModified) ||
			((type & WorkspaceFile::TYPE_UNCHANGED) && !scanUnchanged))
		{
			getFiles().remove(fname);
			continue;
		}
		else
			add_missing = true;

		Workspace::filemap_t::iterator it = getFiles().find(fname);

		WorkspaceFile *rf = 0;
		if(add_missing && it==getFiles().end())
		{
			QFileInfo info(wkdir+QDir::separator()+fname);
			rf = new WorkspaceFile(info, type, wkdir);
			getFiles().insert(rf->getFilePath(), rf);
		}

		if(!rf)
		{
			it = getFiles().find(fname);
			Q_ASSERT(it!=getFiles().end());
			rf = *it;
		}

		rf->setType(type);

		QString path = rf->getPath();
		getPaths().insert(path);
	}

	// Check if the repository needs integration
	res.clear();
	fossil().status(res);
	isIntegrated = false;
	foreach(const QString &l, res)
	{
		if(l.trimmed().indexOf("INTEGRATE")==0)
		{
			isIntegrated = true;
			break;
		}
	}

	// Load the stashes, branches and tags
	fossil().stashList(getStashes());

	fossil().branchList(branchList, branchList);

	fossil().tagList(tags);
	// Fossil includes the branches in the tag list
	// So remove them
	foreach(const QString &name, branchList)
		tags.remove(name);

_done:
	uiCallback.endProcess();
}

//------------------------------------------------------------------------------
bool Workspace::addRemote(const QUrl& url, const QString& name)
{
	if(remotes.contains(url))
		return false;

	Q_ASSERT(url.password().isEmpty());

	Remote r(name, url);
	remotes.insert(r.name, r);
	return true;
}

//------------------------------------------------------------------------------
bool Workspace::removeRemote(const QUrl& url)
{
	return remotes.remove(url) > 0;
}

//------------------------------------------------------------------------------
bool Workspace::setRemoteDefault(const QUrl& url)
{
	Q_ASSERT(url.password().isEmpty());

	bool found = false;
	for(remote_map_t::iterator it=remotes.begin(); it!=remotes.end(); ++it)
	{
		if(it->url == url)
		{
			it->isDefault = true;
			found = true;
		}
		else
			it->isDefault = false;
	}
	return found;
}

//------------------------------------------------------------------------------
const QUrl & Workspace::getRemoteDefault() const
{
	return fossil().getDefaultRemoteUrl();
}

//------------------------------------------------------------------------------
Remote * Workspace::findRemote(const QUrl& url)
{
	remote_map_t::iterator it = remotes.find(url);
	if(it!=remotes.end())
		return &(*it);
	return NULL;
}

