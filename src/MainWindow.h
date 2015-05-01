#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include "SettingsDialog.h"
#include "Workspace.h"

namespace Ui {
	class MainWindow;
}

//////////////////////////////////////////////////////////////////////////
// MainWindow
//////////////////////////////////////////////////////////////////////////
class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(Settings &_settings, QWidget *parent = 0, QString *workspacePath = 0);
	~MainWindow();
	bool diffFile(const QString& repoFile);
	void fullRefresh();

private:
	bool refresh();
	void scanWorkspace();
	void applySettings();
	void updateSettings();
	const QString &getCurrentWorkspace();
	void setCurrentWorkspace(const QString &workspace);
	void log(const QString &text, bool isHTML=false);
	void setStatus(const QString &text);
	bool uiRunning() const;
	void getSelectionFilenames(QStringList &filenames, int includeMask=WorkspaceFile::TYPE_ALL, bool allIfEmpty=false);
	void getFileViewSelection(QStringList &filenames, int includeMask=WorkspaceFile::TYPE_ALL, bool allIfEmpty=false);
	void getDirViewSelection(QStringList &filenames, int includeMask=WorkspaceFile::TYPE_ALL, bool allIfEmpty=false);
	void getStashViewSelection(QStringList &stashNames);
	void getSelectionPaths(stringset_t &paths);
	void getAllFilenames(QStringList &filenames, int includeMask=WorkspaceFile::TYPE_ALL);
	bool startUI();
	void stopUI();
	void enableActions(bool on);
	void addWorkspace(const QString &dir);
	void rebuildRecent();
	bool openWorkspace(const QString &path);
	void loadFossilSettings();
	QString getFossilHttpAddress();
	void updateDirView();
	void updateFileView();
	void selectRootDir();
	void fossilBrowse(const QString &fossilUrl);
	void dragEnterEvent(class QDragEnterEvent *event);
	void dropEvent(class QDropEvent *event);
	void setBusy(bool busy);
	virtual QMenu *createPopupMenu();

	enum ViewMode
	{
		VIEWMODE_LIST,
		VIEWMODE_TREE
	};

private slots:
	// Manual slots.
	// Use a different naming scheme to prevent warnings from Qt's automatic signaling
	void onOpenRecent();
	void onWorkspaceTreeViewSelectionChanged(const class QItemSelection &selected, const class QItemSelection &deselected);
	void onFileViewDragOut();
	void onAbort();

	// Designer slots
	void on_actionRefresh_triggered();
	void on_actionDiff_triggered();
	void on_actionFossilUI_triggered();
	void on_actionQuit_triggered();
	void on_actionTimeline_triggered();
	void on_actionHistory_triggered();
	void on_actionClearLog_triggered();
	void on_fileTableView_doubleClicked(const QModelIndex &index);
	void on_workspaceTreeView_doubleClicked(const QModelIndex &index);
	void on_actionOpenFile_triggered();
	void on_actionPush_triggered();
	void on_actionPull_triggered();
	void on_actionCommit_triggered();
	void on_actionAdd_triggered();
	void on_actionDelete_triggered();
	void on_actionRevert_triggered();
	void on_actionOpenContaining_triggered();
	void on_actionRename_triggered();
	void on_actionUndo_triggered();
	void on_actionAbout_triggered();
	void on_actionUpdate_triggered();
	void on_actionSettings_triggered();
	void on_actionViewUnchanged_triggered();
	void on_actionViewModified_triggered();
	void on_actionViewUnknown_triggered();
	void on_actionViewIgnored_triggered();
	void on_actionViewAsList_triggered();
	void on_actionOpenFolder_triggered();
	void on_actionRenameFolder_triggered();
	void on_actionNewRepository_triggered();
	void on_actionOpenRepository_triggered();
	void on_actionCloseRepository_triggered();
	void on_actionCloneRepository_triggered();
	void on_actionNewStash_triggered();
	void on_actionApplyStash_triggered();
	void on_actionDeleteStash_triggered();
	void on_actionDiffStash_triggered();
	void on_textBrowser_customContextMenuRequested(const QPoint &pos);
	void on_fileTableView_customContextMenuRequested(const QPoint &pos);
	void on_workspaceTreeView_customContextMenuRequested(const QPoint &pos);

private:
	class MainWinUICallback : public UICallback
	{
	public:
		MainWinUICallback() : mainWindow(0)
		{}

		void init(class MainWindow *mainWindow)
		{
			this->mainWindow = mainWindow;
		}

		virtual void logText(const QString& text, bool isHTML);
		virtual void beginProcess(const QString& text);
		virtual void updateProcess(const QString& text);
		virtual void endProcess();
		virtual QMessageBox::StandardButton Query(const QString &title, const QString &query, QMessageBox::StandardButtons buttons);


	private:
		class MainWindow *mainWindow;
	};

	friend class MainWinUICallback;

	enum
	{
		MAX_RECENT=5
	};

	Ui::MainWindow		*ui;
	class QAction		*recentWorkspaceActs[MAX_RECENT];
	class QProgressBar	*progressBar;
	class QShortcut		*abortShortcut;
	QMenu				*menuWorkspace;
	QMenu				*menuStashes;

	bool				operationAborted;
	stringset_t			selectedDirs;	// The directory selected in the tree

	Workspace			workspace;
	Workspace &			getWorkspace() { return workspace; }

	Fossil &			fossil() { return workspace.fossil(); }
	const Fossil &		fossil() const { return workspace.fossil(); }

	Settings			&settings;
	QStringList			workspaceHistory;

	MainWinUICallback	uiCallback;

	ViewMode			viewMode;
};

#endif // MAINWINDOW_H

