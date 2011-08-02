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
	enum Status
	{
		STATUS_UNKNOWN,
		STATUS_UNCHAGED,
		STATUS_EDITTED
	};

	QFileInfo fileinfo;
	Status	status;
	QString filename;

	QString getRelativeFilename(const QString &path)
	{
		QString abs_base_dir = QDir(path).absolutePath();

		QString relative = fileinfo.absoluteFilePath();
		int index = relative.indexOf(abs_base_dir);
		if(index<0)
			return QString("");

		return relative.right(relative.length() - abs_base_dir.length()-1);
	}
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

private slots:
	void on_actionRefresh_triggered();
	void on_actionOpen_triggered();

	void on_actionDiff_triggered();

	void on_actionFossilUI_toggled(bool arg1);

public slots:
	void on_tableView_customContextMenuRequested(const QPoint &pos);


private:
    Ui::MainWindow *ui;
	QStandardItemModel itemModel;
	QString settingsFile;

	QString fossilPath;
	QStringList workspaces;
	typedef QMap<QString, FileEntry> filemap_t;
	filemap_t workspaceFiles;
	int currentWorkspace;

	QProcess fossilUI;
};

#endif // MAINWINDOW_H
