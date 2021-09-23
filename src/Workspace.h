#ifndef WORKSPACE_H
#define WORKSPACE_H

#include "Fossil.h"
#include "Utils.h"
#include "WorkspaceCommon.h"
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QSet>
#include <QSettings>
#include <QStandardItemModel>

//////////////////////////////////////////////////////////////////////////
// Workspace
//////////////////////////////////////////////////////////////////////////
class Workspace
{
public:
    Workspace();
    ~Workspace();

    void clearState();

    Fossil &fossil() { return bridge; }
    const Fossil &fossil() const { return bridge; }

    const QString &getPath() const { return fossil().getWorkspacePath(); }
    bool switchWorkspace(const QString &workspace, QSettings &store);
    void scanWorkspace(bool scanLocal, bool scanIgnored, bool scanModified, bool scanUnchanged, const QStringList &ignorePatterns, UICallback &uiCallback);

    QStandardItemModel &getFileModel() { return repoFileModel; }
    QStandardItemModel &getTreeModel() { return repoTreeModel; }

    filemap_t &getFiles() { return workspaceFiles; }
    stringset_t &getPaths() { return pathSet; }
    pathstate_map_t &getPathState() { return pathState; }
    stashmap_t &getStashes() { return stashMap; }
    QStringMap &getTags() { return tags; }
    QStringList &getBranches() { return branchNames; }
    bool otherChanges() const { return isIntegrated; }
    const QString &getCurrentRevision() const { return fossil().getCurrentRevision(); }
    const QStringList &getActiveTags() const { return fossil().getActiveTags(); }
    const QString &getProjectName() const { return fossil().getProjectName(); }

    // Remotes
    const remote_map_t &getRemotes() const { return remotes; }
    bool addRemote(const QUrl &url, const QString &name);
    bool removeRemote(const QUrl &url);
    bool setRemoteDefault(const QUrl &url);
    QUrl getRemoteDefault() const;
    Remote *findRemote(const QUrl &url);

    void storeWorkspace(QSettings &store);

    // Fossil Wrappers
    void Init(UICallback *callback, const QString &exePath) { fossil().Init(callback, exePath); }

    bool create(const QString &repositoryPath, const QString &workspacePath) { return fossil().createWorkspace(repositoryPath, workspacePath); }

    bool createRepository(const QString &repositoryPath) { return fossil().createRepository(repositoryPath); }

    WorkspaceState getState() { return fossil().getWorkspaceState(); }

    bool close(bool force = false) { return fossil().closeWorkspace(force); }

    bool cloneRepository(const QString &repository, const QUrl &url, const QUrl &proxyUrl) { return fossil().cloneRepository(repository, url, proxyUrl); }

    bool push(const QUrl &url) { return fossil().pushWorkspace(url); }

    bool pull(const QUrl &url) { return fossil().pullWorkspace(url); }

    bool update(QStringList &result, const QString &revision, bool explainOnly) { return fossil().updateWorkspace(result, revision, explainOnly); }

    bool undo(QStringList &result, bool explainOnly) { return fossil().undoWorkspace(result, explainOnly); }

    bool diffFile(const QString &repoFile, bool graphical) { return fossil().diffFile(repoFile, graphical); }

    bool commitFiles(const QStringList &fileList, const QString &comment, const QString &newBranchName, bool isPrivateBranch)
    {
        return fossil().commitFiles(fileList, comment, newBranchName, isPrivateBranch);
    }

    bool addFiles(const QStringList &fileList) { return fossil().addFiles(fileList); }

    bool removeFiles(const QStringList &fileList, bool deleteLocal) { return fossil().removeFiles(fileList, deleteLocal); }

    bool addRemoveFiles() { return fossil().addRemoveFiles(); }

    bool revertFiles(const QStringList &fileList) { return fossil().revertFiles(fileList); }

    bool renameFile(const QString &beforePath, const QString &afterPath, bool renameLocal) { return fossil().renameFile(beforePath, afterPath, renameLocal); }

    // Stashes
    bool stashNew(const QStringList &fileList, const QString &name, bool revert) { return fossil().stashNew(fileList, name, revert); }

    bool stashList(stashmap_t &stashes) { return fossil().stashList(stashes); }

    bool stashApply(const QString &name) { return fossil().stashApply(name); }

    bool stashDrop(const QString &name) { return fossil().stashDrop(name); }

    bool stashDiff(const QString &name) { return fossil().stashDiff(name); }

    // Tags
    bool tagList(QStringMap &tags) { return fossil().tagList(tags); }

    bool tagNew(const QString &name, const QString &revision) { return fossil().tagNew(name, revision); }

    bool tagDelete(const QString &name, const QString &revision) { return fossil().tagDelete(name, revision); }

    // Branches
    bool branchList(QStringList &branches, QStringList &activeBranches) { return fossil().branchList(branches, activeBranches); }

    bool branchNew(const QString &name, const QString &revisionBasis, bool isPrivate = false) { return fossil().branchNew(name, revisionBasis, isPrivate); }

    bool branchMerge(QStringList &res, const QString &revision, bool integrate, bool force, bool testOnly) { return fossil().branchMerge(res, revision, integrate, force, testOnly); }

    bool getInterfaceVersion(QString &version) { return fossil().getExeVersion(version); }

private:
    static bool scanDirectory(QFileInfoList &entries, const QString &dirPath, const QString &baseDir, const QStringList &ignorePatterns, UICallback &uiCallback);

private:
    Fossil bridge;
    filemap_t workspaceFiles;
    stringset_t pathSet;
    pathstate_map_t pathState;
    stashmap_t stashMap;
    QStringList branchNames;
    QStringMap tags;
    remote_map_t remotes;
    bool isIntegrated;

    QStandardItemModel repoFileModel;
    QStandardItemModel repoTreeModel;
};

#endif  // WORKSPACE_H
