#ifndef BRIDGE_H
#define BRIDGE_H

class QStringList;
#include <QString>
#include <QObject>
#include <QProcess>
#include <QTextBrowser>


class Bridge : public QObject
{
public:
	Bridge()
	: QObject(0)
	, parentWidget(0)
	, abortOperation(false)
	, logTextBrowser(0)
	{
	}


	enum RunFlags
	{
		RUNFLAGS_NONE			= 0<<0,
		RUNFLAGS_SILENT_INPUT	= 1<<0,
		RUNFLAGS_SILENT_OUTPUT	= 1<<1,
		RUNFLAGS_SILENT_ALL		= RUNFLAGS_SILENT_INPUT | RUNFLAGS_SILENT_OUTPUT,
		RUNFLAGS_DETACHED		= 1<<2
	};

	typedef void(*log_callback_t)(QTextBrowser *textBrowser, const QString &text, bool isHTML);


	void Init(QWidget *parent, log_callback_t callback, QTextBrowser *textBrowser, const QString &fossPath, const QString &workspace)
	{
		parentWidget = parent;
		logCallback = callback;
		logTextBrowser = textBrowser;

		fossilPath = fossPath;
		currentWorkspace = workspace;
	}

	bool runFossil(const QStringList &args, QStringList *output=0, int runFlags=RUNFLAGS_NONE);
	bool runFossilRaw(const QStringList &args, QStringList *output, int *exitCode, int runFlags);

	static bool isWorkspace(const QString &path);
	enum RepoStatus
	{
		REPO_OK,
		REPO_NOT_FOUND,
		REPO_OLD_SCHEMA
	};

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

	bool uiRunning() const;
	bool startUI(const QString &httpPort);
	void stopUI();


private:
	void log(const QString &text, bool isHTML=false)
	{
		if(logCallback)
			(*logCallback)(logTextBrowser, text, isHTML);
	}


	QString getFossilPath();

	QWidget				*parentWidget;	// fixme
	bool				abortOperation;	// FIXME: No GUI for it yet

	log_callback_t		logCallback;
	QTextBrowser		*logTextBrowser;
	QString				currentWorkspace;
	QString				fossilPath;		// The value from the settings
	QString				repositoryFile;
	QString				projectName;

	QProcess			fossilUI;
};


#endif // BRIDGE_H
