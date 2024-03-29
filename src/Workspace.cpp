#include "Workspace.h"
#include "Utils.h"
#include <QCoreApplication>

//-----------------------------------------------------------------------------
Workspace::Workspace() {}
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
    foreach (WorkspaceFile *r, getFiles())
        delete r;

    getFiles().clear();
    getPaths().clear();
    pathState.clear();
    stashMap.clear();
    branchNames.clear();
    tags.clear();
    isIntegrated = false;
}

//------------------------------------------------------------------------------
void Workspace::storeWorkspace(QSettings &store)
{
    QString workspace = fossil().getWorkspacePath();
    if (workspace.isEmpty())
        return;

    store.beginGroup("Remotes");
    QString workspace_hash = HashString(QDir::toNativeSeparators(workspace));

    store.beginWriteArray(workspace_hash);
    int index = 0;
    for (auto &re : remotes)
    {
        store.setArrayIndex(index);
        store.setValue("Name", re.name);
        QUrl url = re.url;
        url.setPassword("");
        store.setValue("Url", url);
        if (re.isDefault)
            store.setValue("Default", re.isDefault);
        else
            store.remove("Default");
    }
    store.endArray();
    store.endGroup();
}

//------------------------------------------------------------------------------
bool Workspace::switchWorkspace(const QString &workspace, QSettings &store)
{
    // Save Remotes
    storeWorkspace(store);
    clearState();
    remotes.clear();

    fossil().setWorkspace("");
    if (workspace.isEmpty())
        return true;

    QString new_workspace = QFileInfo(workspace).absoluteFilePath();

    if (!QDir::setCurrent(new_workspace))
        return false;

    fossil().setWorkspace(new_workspace);

    // Load Remotes
    QString workspace_hash = HashString(QDir::toNativeSeparators(new_workspace));

    store.beginGroup("Remotes");
    int num_remotes = store.beginReadArray(workspace_hash);
    for (int i = 0; i < num_remotes; ++i)
    {
        store.setArrayIndex(i);

        QString name = store.value("Name").toString();
        QUrl url = store.value("Url").toUrl();
        bool def = store.value("Default", false).toBool();
        addRemote(url, name);
        if (def)
            setRemoteDefault(url);
    }
    store.endArray();
    store.endGroup();

#if 0  // FIXME: Disabled this because if fossil's remote does not match exactly what we have stored (url and username), it will be automatically added every-time
	// Add the default url from fossil
	QUrl default_remote;
	if(fossil().getRemoteUrl(default_remote) && default_remote.isValid() && !default_remote.isEmpty())
	{
		default_remote.setPassword("");

		// Add Default remote if not available already
		if(findRemote(default_remote)==NULL)
		{
			addRemote(default_remote, UrlToStringDisplay(default_remote));
			if(getRemoteDefault().isEmpty())
				setRemoteDefault(default_remote);
		}
	}
#endif

    return true;
}

//------------------------------------------------------------------------------
bool Workspace::scanDirectory(QFileInfoList &entries, const QString &dirPath, const QString &baseDir, const QStringList &ignorePatterns, UICallback &uiCallback)
{
    QDir dir(dirPath);

    uiCallback.updateProcess(dirPath);

    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
    for (auto &info : list)
    {
        if (uiCallback.processAborted())
            return false;

        QString filepath = info.filePath();
        QString rel_path = filepath;
        rel_path.remove(baseDir + PATH_SEPARATOR);

        // Skip ignored files
        if (!ignorePatterns.isEmpty() && QDir::match(ignorePatterns, rel_path))
            continue;

        if (info.isDir())
        {
            if (!scanDirectory(entries, filepath, baseDir, ignorePatterns, uiCallback))
                return false;
        }
        else
            entries.push_back(info);
    }
    return true;
}

//------------------------------------------------------------------------------
static bool StringLengthDescending(const QString &l, const QString &r)
{
    return l.length() > r.length();
}

//------------------------------------------------------------------------------
void Workspace::scanWorkspace(bool scanLocal, bool scanIgnored, bool scanModified, bool scanUnchanged, const QStringList &ignorePatterns, UICallback &uiCallback)
{
    // Scan all workspace files
    QFileInfoList all_files;
    QString wkdir = fossil().getWorkspacePath();

    if (wkdir.isEmpty())
        return;

    // Retrieve the status of files tracked by fossil
    QStringList res;
    if (!fossil().listFiles(res))
        return;

    bool scan_files = scanLocal;

    clearState();

    QStringList paths;

    uiCallback.beginProcess("");
    if (scan_files)
    {
        QCoreApplication::processEvents();

        QStringList ignore;
        if (!scanIgnored)
            ignore = ignorePatterns;

        if (!scanDirectory(all_files, wkdir, wkdir, ignore, uiCallback))
            goto _done;

        for (auto &all_file : all_files)
        {
            QString filename = all_file.fileName();
            QString fullpath = all_file.absoluteFilePath();

            // Skip fossil files
            if (filename == FOSSIL_CHECKOUT1 || filename == FOSSIL_CHECKOUT2 || (!fossil().getRepositoryFile().isEmpty() && QFileInfo(fullpath) == QFileInfo(fossil().getRepositoryFile())))
                continue;

            WorkspaceFile::Type type = WorkspaceFile::TYPE_UNKNOWN;

            WorkspaceFile *rf = new WorkspaceFile(all_file, type, wkdir);
            const QString &path = rf->getPath();
            getFiles().insert(rf->getFilePath(), rf);
            getPaths().insert(path);

            // Add or merge file state into directory state
            pathstate_map_t::iterator state_it = pathState.find(path);
            if (state_it != pathState.end())
                state_it.value() = static_cast<WorkspaceFile::Type>(state_it.value() | type);
            else
            {
                pathState.insert(path, type);
                paths.append(path);  // keep path in list for depth sort
            }
        }
    }
    uiCallback.endProcess();

    uiCallback.beginProcess(QObject::tr("Updating..."));

    // Update Files and Directories
    for (auto &re : res)
    {
        QString line = re.trimmed();
        if (line.length() == 0)
            continue;

        int space_index = line.indexOf(' ');
        if (space_index == -1)
            continue;

        QString status_text = line.left(space_index);
        QString fname = line.right(line.length() - space_index).trimmed();
        WorkspaceFile::Type type = WorkspaceFile::TYPE_UNKNOWN;

        // Generate a RepoFile for all non-existant fossil files
        // or for all files if we skipped scanning the workspace
        bool add_missing = !scan_files;

        if (status_text == "EDITED")
            type = WorkspaceFile::TYPE_EDITTED;
        else if (status_text == "ADDED")
            type = WorkspaceFile::TYPE_ADDED;
        else if (status_text == "DELETED")
        {
            type = WorkspaceFile::TYPE_DELETED;
            add_missing = true;
        }
        else if (status_text == "MISSING")
        {
            type = WorkspaceFile::TYPE_MISSING;
            add_missing = true;
        }
        else if (status_text == "RENAMED")
            type = WorkspaceFile::TYPE_RENAMED;
        else if (status_text == "UNCHANGED")
            type = WorkspaceFile::TYPE_UNCHANGED;
        else if (status_text == "CONFLICT")
            type = WorkspaceFile::TYPE_CONFLICTED;
        else if (status_text == "UPDATED_BY_MERGE" || status_text == "ADDED_BY_MERGE" || status_text == "ADDED_BY_INTEGRATE" || status_text == "UPDATED_BY_INTEGRATE")
            type = WorkspaceFile::TYPE_MERGED;

        // Filter unwanted file types
        if (((type & WorkspaceFile::TYPE_MODIFIED) && !scanModified) || ((type & WorkspaceFile::TYPE_UNCHANGED) && !scanUnchanged))
        {
            getFiles().remove(fname);
            continue;
        }
        else
            add_missing = true;

        filemap_t::iterator it = getFiles().find(fname);

        WorkspaceFile *rf = 0;
        if (add_missing && it == getFiles().end())
        {
            QFileInfo info(wkdir + QDir::separator() + fname);
            rf = new WorkspaceFile(info, type, wkdir);
            getFiles().insert(rf->getFilePath(), rf);
        }

        if (!rf)
        {
            it = getFiles().find(fname);
            Q_ASSERT(it != getFiles().end());
            rf = *it;
        }

        rf->setType(type);

        QString path = rf->getPath();
        getPaths().insert(path);

        // Add or merge file state into directory state
        pathstate_map_t::iterator state_it = pathState.find(path);
        if (state_it != pathState.end())
            state_it.value() = static_cast<WorkspaceFile::Type>(state_it.value() | type);
        else
        {
            pathState.insert(path, type);
            paths.append(path);  // keep path in list for depth sort
        }
    }

    // Sort paths, so that children (longer path) are before parents (shorter path)
    std::sort(paths.begin(), paths.end(), StringLengthDescending);
    foreach (const QString &p, paths)
    {
        pathstate_map_t::iterator state_it = pathState.find(p);
        Q_ASSERT(state_it != pathState.end());
        WorkspaceFile::Type state = state_it.value();

        // Propagate child dir state to parents
        QString parent_path = p;
        while (!parent_path.isEmpty())
        {
            // Extract parent path
            int sep_index = parent_path.lastIndexOf(PATH_SEPARATOR);
            if (sep_index >= 0)
                parent_path = parent_path.left(sep_index);
            else
                parent_path = "";

            // Merge path of child to parent
            pathstate_map_t::iterator state_it2 = pathState.find(parent_path);
            if (state_it2 != pathState.end())
                state_it2.value() = static_cast<WorkspaceFile::Type>(state_it2.value() | state);
            else
                pathState.insert(parent_path, state);
        }
    }

    // Check if the repository needs integration
    res.clear();
    fossil().statusWorkspace(res);
    isIntegrated = false;
    foreach (const QString &l, res)
    {
        if (l.trimmed().indexOf("INTEGRATE") == 0)
        {
            isIntegrated = true;
            break;
        }
    }

    // Load the stashes, branches and tags
    fossil().stashList(getStashes());

    fossil().branchList(branchNames, branchNames);

    fossil().tagList(tags);
    // Fossil includes the branches in the tag list
    // So remove them
    foreach (const QString &name, branchNames)
        tags.remove(name);

_done:
    uiCallback.endProcess();
}

//------------------------------------------------------------------------------
bool Workspace::addRemote(const QUrl &url, const QString &name)
{
    if (remotes.contains(url))
        return false;

    Q_ASSERT(url.password().isEmpty());

    Remote r(name, url);
    remotes.insert(url, r);
    return true;
}

//------------------------------------------------------------------------------
bool Workspace::removeRemote(const QUrl &url)
{
    return remotes.remove(url) > 0;
}

//------------------------------------------------------------------------------
bool Workspace::setRemoteDefault(const QUrl &url)
{
    Q_ASSERT(url.password().isEmpty());

    const QString &url_str = url.toString();

    bool found = false;
    for (auto &remote : remotes)
    {
        if (remote.url.toString() == url_str)  // FIXME: Use strings as QUrl to QUrl comparisons sometime fail!?
        {
            remote.isDefault = true;
            found = true;
        }
        else
            remote.isDefault = false;
    }
    return found;
}

//------------------------------------------------------------------------------
QUrl Workspace::getRemoteDefault() const
{
    for (const auto &remote : remotes)
    {
        if (remote.isDefault)
            return remote.url;
    }

    return QUrl();
}

//------------------------------------------------------------------------------
Remote *Workspace::findRemote(const QUrl &url)
{
    remote_map_t::iterator it = remotes.find(url);
    if (it != remotes.end())
        return &(*it);
    return NULL;
}
