#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QStringList>
#include <QMap>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include "SettingsDialog.h"

namespace Ui {
    class MainWindow;
}

class QStringList;

struct RepoFile
{
	enum EntryType
	{
		TYPE_UNKNOWN		= 1<<0,
		TYPE_UNCHANGED		= 1<<1,
		TYPE_EDITTED		= 1<<2,
		TYPE_ADDED			= 1<<3,
		TYPE_DELETED		= 1<<4,
		TYPE_MISSING		= 1<<5,
		TYPE_RENAMED		= 1<<6,
		TYPE_MODIFIED		= TYPE_EDITTED|TYPE_ADDED|TYPE_DELETED|TYPE_MISSING|TYPE_RENAMED,
		TYPE_REPO			= TYPE_UNCHANGED|TYPE_MODIFIED,
		TYPE_ALL			= TYPE_UNKNOWN|TYPE_REPO
	};

	void set(QFileInfo &info, EntryType type, const QString &repoPath)
	{
		FileInfo = info;
		Type = type;
		Filename = getRelativeFilename(repoPath);
	}

	bool isType(EntryType t) const
	{
		return Type == t;
	}

	void setType(EntryType t)
	{
		Type = t;
	}

	EntryType getType() const
	{
		return Type;
	}

	QFileInfo getFileInfo() const
	{
		return FileInfo;
	}

	bool isRepo() const
	{
		return Type == TYPE_UNCHANGED || Type == TYPE_EDITTED;
	}

	const QString &getFilename() const
	{
		return Filename;
	}

	QString getRelativeFilename(const QString &path)
	{
		QString abs_base_dir = QDir(path).absolutePath();

		QString relative = FileInfo.absoluteFilePath();
		int index = relative.indexOf(abs_base_dir);
		if(index<0)
			return QString("");

		return relative.right(relative.length() - abs_base_dir.length()-1);
	}

private:
	QFileInfo	FileInfo;
	EntryType	Type;
	QString		Filename;
};



class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
	bool diffFile(QString repoFile);

private:
	bool refresh();
	void scanWorkspace();
	bool runFossil(const QStringList &args, QStringList *output=0, bool silent=false, bool detached=false);
	bool runFossilRaw(const QStringList &args, QStringList *output=0, int *exitCode=0, bool silent=false, bool detached=false);
	void loadSettings();
	void saveSettings();
	const QString &getCurrentWorkspace();
	void setCurrentWorkspace(const QString &workspace);
	void log(const QString &text);
	void setStatus(const QString &text);
	bool uiRunning() const { return fossilUI.state() == QProcess::Running; }
	void getSelectionFilenames(QStringList &filenames, int includeMask=RepoFile::TYPE_ALL, bool allIfEmpty=false);
	bool startUI();
	void stopUI();
	void enableActions(bool on);
	void addWorkspace(const QString &dir);
	void rebuildRecent();
	bool openWorkspace(const QString &dir);
	QString getFossilPath();

	enum RepoStatus
	{
		REPO_OK,
		REPO_NOT_FOUND,
		REPO_OLD_SCHEMA
	};

	RepoStatus getRepoStatus();

private slots:
	// Manual slots
	void onOpenRecent();

	// Designer slots
	void on_actionRefresh_triggered();
	void on_actionOpen_triggered();
	void on_actionDiff_triggered();
	void on_actionFossilUI_toggled(bool arg1);
	void on_actionQuit_triggered();
	void on_actionTimeline_triggered();
	void on_actionHistory_triggered();
	void on_actionClearLog_triggered();
	void on_tableView_doubleClicked(const QModelIndex &index);
	void on_actionOpenFile_triggered();
	void on_actionPush_triggered();
	void on_actionPull_triggered();
	void on_actionCommit_triggered();
	void on_actionAdd_triggered();
	void on_actionDelete_triggered();
	void on_actionRevert_triggered();
	void on_actionNew_triggered();
	void on_actionClone_triggered();
	void on_actionOpenContaining_triggered();
	void on_actionRename_triggered();
	void on_actionUndo_triggered();
	void on_actionAbout_triggered();
	void on_actionUpdate_triggered();
	void on_actionSettings_triggered();
	void on_actionSyncSettings_triggered();
	void on_actionViewUnchanged_triggered();
	void on_actionViewModified_triggered();
	void on_actionViewUnknown_triggered();

private:
	enum
	{
		MAX_RECENT=5
	};

	Ui::MainWindow		*ui;
	QStandardItemModel	itemModel;
	QProcess			fossilUI;
	class QAction		*recentWorkspaceActs[MAX_RECENT];
	class QLabel		*statusLabel;
	bool				fossilAbort;	// FIXME: No GUI for it yet

	Settings			settings;
	QString				projectName;
	QString				repositoryFile;
	QStringList			workspaceHistory;
	QString				currentWorkspace;
	QStringList			commitMessages;

	// Repo State
	typedef QMap<QString, RepoFile> filemap_t;
	filemap_t			workspaceFiles;
};

#endif // MAINWINDOW_H
