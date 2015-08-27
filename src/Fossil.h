#ifndef FOSSIL_H
#define FOSSIL_H

class QStringList;
#include <QString>
#include <QStringList>
#include <QUrl>
#include "LoggedProcess.h"
#include "Utils.h"

typedef QMap<QString, QString> stashmap_t;

class Fossil
{
public:
	enum WorkspaceState
	{
		WORKSPACE_STATE_OK,
		WORKSPACE_STATE_NOTFOUND,
		WORKSPACE_STATE_OLDSCHEMA
	};

	Fossil();
	void Init(UICallback *callback);

	// Repositories
	bool createRepository(const QString &repositoryPath);
	bool cloneRepository(const QString &repository, const QUrl &url, const QUrl &proxyUrl);

	// Workspace
	bool createWorkspace(const QString &repositoryPath, const QString& workspacePath);
	bool closeWorkspace();
	void setWorkspace(const QString &_workspacePath);
	bool pushWorkspace(const QUrl& url);
	bool pullWorkspace(const QUrl& url);
	bool undoWorkspace(QStringList& result, bool explainOnly);
	bool updateWorkspace(QStringList& result, const QString& revision, bool explainOnly);
	bool statusWorkspace(QStringList& result);
	WorkspaceState getWorkspaceState();
	static bool isWorkspace(const QString &path);

	// Workspace Information
	const QString &getProjectName() const {	return projectName;	}
	const QString &getRepositoryFile() const {	return repositoryFile; }
	const QString &getWorkspacePath() const { return workspacePath;	}

	// Files
	bool listFiles(QStringList &files);
	bool diffFile(const QString &repoFile, bool graphical);
	bool commitFiles(const QStringList &fileList, const QString &comment, const QString& newBranchName, bool isPrivateBranch);
	bool addFiles(const QStringList& fileList);
	bool removeFiles(const QStringList& fileList, bool deleteLocal);
	bool revertFiles(const QStringList& fileList);
	bool renameFile(const QString& beforePath, const QString& afterPath, bool renameLocal);

	// Settings
	bool getSettings(QStringList& result);
	bool setSetting(const QString &name, const QString &value, bool global);

	// Remotes
	bool setRemoteUrl(const QUrl& url);
	bool getRemoteUrl(QUrl &url);

	// Stashes
	bool stashNew(const QStringList& fileList, const QString& name, bool revert);
	bool stashList(stashmap_t &stashes);
	bool stashApply(const QString& name);
	bool stashDrop(const QString& name);
	bool stashDiff(const QString& name);

	// Tags
	bool tagList(QStringMap& tags);
	bool tagNew(const QString& name, const QString& revision);
	bool tagDelete(const QString& name, const QString& revision);

	// Branches
	bool branchList(QStringList& branches, QStringList& activeBranches);
	bool branchNew(const QString& name, const QString& revisionBasis, bool isPrivate=false);
	bool branchMerge(QStringList& res, const QString& revision, bool integrate, bool force, bool testOnly);

	const QString &getCurrentRevision() const { return currentRevision; }
	const QStringList &getActiveTags() const { return activeTags; }

	// UI
	bool uiRunning() const;
	bool startUI(const QString &httpPort);
	void stopUI();
	const QString &getUIHttpPort() const { return fossilUIPort; }
	QString getUIHttpAddress() const;

	// Fossil executable
	void setExePath(const QString &path) { fossilPath = path; }
	bool getExeVersion(QString &version);

private:
	enum RunFlags
	{
		RUNFLAGS_NONE			= 0<<0,
		RUNFLAGS_SILENT_INPUT	= 1<<0,
		RUNFLAGS_SILENT_OUTPUT	= 1<<1,
		RUNFLAGS_SILENT_ALL		= RUNFLAGS_SILENT_INPUT | RUNFLAGS_SILENT_OUTPUT,
		RUNFLAGS_DETACHED		= 1<<2,
		RUNFLAGS_DEBUG			= 1<<3,
	};

	void setRepositoryFile(const QString &filename) { repositoryFile = filename; }
	bool runFossil(const QStringList &args, QStringList *output=0, int runFlags=RUNFLAGS_NONE);
	bool runFossilRaw(const QStringList &args, QStringList *output, int *exitCode, int runFlags);

	void log(const QString &text, bool isHTML=false)
	{
		if(uiCallback)
			uiCallback->logText(text, isHTML);
	}

	QString	getFossilPath();

	UICallback			*uiCallback;
	QString				workspacePath;
	QString				fossilPath;		// The value from the settings
	QString				repositoryFile;
	QString				projectName;
	QString				currentRevision;
	QStringList			activeTags;
	LoggedProcess		fossilUI;
	QString				fossilUIPort;
};


#endif // FOSSIL_H
