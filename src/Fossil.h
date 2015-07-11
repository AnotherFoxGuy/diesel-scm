#ifndef FOSSIL_H
#define FOSSIL_H

class QStringList;
#include <QString>
#include <QStringList>
#include <QUrl>
#include "LoggedProcess.h"
#include "Utils.h"

typedef QMap<QString, QString> stashmap_t;

enum RunFlags
{
	RUNFLAGS_NONE			= 0<<0,
	RUNFLAGS_SILENT_INPUT	= 1<<0,
	RUNFLAGS_SILENT_OUTPUT	= 1<<1,
	RUNFLAGS_SILENT_ALL		= RUNFLAGS_SILENT_INPUT | RUNFLAGS_SILENT_OUTPUT,
	RUNFLAGS_DETACHED		= 1<<2,
	RUNFLAGS_DEBUG			= 1<<3,
};

enum RepoStatus
{
	REPO_OK,
	REPO_NOT_FOUND,
	REPO_OLD_SCHEMA
};

class Fossil
{
public:
	Fossil()
	: operationAborted(false)
	, uiCallback(0)
	{
	}

	void Init(UICallback *callback)
	{
		uiCallback = callback;
		fossilPath.clear();
		currentWorkspace.clear();
	}

	bool runFossil(const QStringList &args, QStringList *output=0, int runFlags=RUNFLAGS_NONE);
	bool runFossilRaw(const QStringList &args, QStringList *output, int *exitCode, int runFlags);

	static bool isWorkspace(const QString &path);

	RepoStatus getRepoStatus();

	void setCurrentWorkspace(const QString &workspace)
	{
		currentWorkspace = workspace;
	}

	const QString &getCurrentWorkspace() const
	{
		return currentWorkspace;
	}


	const QString &getProjectName() const
	{
		return projectName;
	}

	const QString &getRepositoryFile() const
	{
		return repositoryFile;
	}

	void setRepositoryFile(const QString &filename)
	{
		repositoryFile = filename;
	}

	bool openRepository(const QString &repositoryPath, const QString& workspacePath);
	bool newRepository(const QString &repositoryPath);
	bool closeRepository();
	bool pushRepository(const QUrl& url);
	bool pullRepository(const QUrl& url);
	bool cloneRepository(const QString &repository, const QUrl &url, const QUrl &proxyUrl);
	bool undoRepository(QStringList& result, bool explainOnly);
	bool updateRepository(QStringList& result, const QString& revision, bool explainOnly);
	bool getFossilVersion(QString &version);

	bool uiRunning() const;
	bool startUI(const QString &httpPort);
	void stopUI();

	bool listFiles(QStringList &files);
	bool status(QStringList& result);

	bool diffFile(const QString &repoFile);
	bool commitFiles(const QStringList &fileList, const QString &comment, const QString& newBranchName, bool isPrivateBranch);
	bool addFiles(const QStringList& fileList);
	bool removeFiles(const QStringList& fileList, bool deleteLocal);
	bool revertFiles(const QStringList& fileList);
	bool renameFile(const QString& beforePath, const QString& afterPath, bool renameLocal);
	bool getFossilSettings(QStringList& result);
	bool setFossilSetting(const QString &name, const QString &value, bool global);
	bool setRemoteUrl(const QUrl& url);
	bool getRemoteUrl(QUrl &url);

	bool stashNew(const QStringList& fileList, const QString& name, bool revert);
	bool stashList(stashmap_t &stashes);
	bool stashApply(const QString& name);
	bool stashDrop(const QString& name);
	bool stashDiff(const QString& name);

	void abortOperation() { operationAborted = true; }

	bool tagList(QStringMap& tags);
	bool tagNew(const QString& name, const QString& revision);
	bool tagDelete(const QString& name, const QString& revision);

	bool branchList(QStringList& branches, QStringList& activeBranches);
	bool branchNew(const QString& name, const QString& revisionBasis, bool isPrivate=false);
	bool branchMerge(QStringList& res, const QString& revision, bool integrate, bool force, bool testOnly);

	const QString &getCurrentRevision() const { return currentRevision; }
	const QStringList &getCurrentTags() const { return currentTags; }

	const QString &getUIHttpPort() const { return fossilUIPort; }
	QString getUIHttpAddress() const;

private:
	void log(const QString &text, bool isHTML=false)
	{
		if(uiCallback)
			uiCallback->logText(text, isHTML);
	}

	QString	getFossilPath();

	bool				operationAborted;
	UICallback			*uiCallback;
	QString				currentWorkspace;
	QString				fossilPath;		// The value from the settings
	QString				repositoryFile;
	QString				projectName;
	QString				currentRevision;
	QStringList			currentTags;
	LoggedProcess		fossilUI;
	QString				fossilUIPort;
};


#endif // FOSSIL_H
