#include "Workspace.h"
#include <QCoreApplication>
#include "Utils.h"

//-----------------------------------------------------------------------------
Workspace::~Workspace()
{
	clearState();
}

//------------------------------------------------------------------------------
void Workspace::clearState()
{
	// Dispose RepoFiles
	foreach(WorkspaceFile *r, getFiles())
		delete r;

	getFiles().clear();
	getPaths().clear();
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

		QString status_text = line.left(10).trimmed();
		QString fname = line.right(line.length() - 10).trimmed();
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

	// Load the stash
	fossil().stashList(getStashes());
_done:
	uiCallback.endProcess();
}

