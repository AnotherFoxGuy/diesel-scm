#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QStringList>
#include <QMap>
#include <QFileInfo>
#include <QDir>
#include <QProcess>

namespace Ui {
    class MainWindow;
}

class QStringList;

struct FileEntry
{
	enum EntryType
	{
		TYPE_UNKNOWN	= 1<<0,
		TYPE_UNCHANGED	= 1<<1,
		TYPE_EDITTED	= 1<<2,
		TYPE_ALL		= TYPE_UNKNOWN|TYPE_UNCHANGED|TYPE_EDITTED
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

private:
	void refresh();
	bool runFossil(QStringList &result, const QStringList &args);
	void loadSettings();
	void saveSettings();
	const QString &getCurrentWorkspace() { Q_ASSERT(currentWorkspace<workspaces.size()); return workspaces[currentWorkspace]; }
	void Log(const QString &text);
	bool uiRunning() const { return fossilUI.state() == QProcess::Running; }
	void getSelectionFilenames(QStringList &filenames, int includeMask=FileEntry::TYPE_ALL);
	bool startUI();
	void stopUI();
	void updateStatus();

private slots:
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

private:
	Ui::MainWindow		*ui;
	QStandardItemModel	itemModel;
	QProcess			fossilUI;
	QString				settingsFile;

	QString				projectName;
	QString				repositoryFile;
	QString				fossilPath;
	QStringList			workspaces;
	typedef QMap<QString, FileEntry> filemap_t;
	filemap_t			workspaceFiles;
	int					currentWorkspace;
	QStringList			commitMessages;
};

#endif // MAINWINDOW_H
