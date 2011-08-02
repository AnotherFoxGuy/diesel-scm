#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QFileDialog>
#include <QStandardItem>
#include <QProcess>
#include <QSettings>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
	ui->tableView->setModel(&itemModel);
	ui->tableView->addAction(ui->actionDiff);
	ui->tableView->addAction(ui->actionHistory);
	ui->tableView->addAction(ui->actionAdd);
	ui->tableView->addAction(ui->actionDelete);
	ui->tableView->addAction(ui->actionRename);

	settingsFile = QDir::homePath() + QDir::separator() + ".fuelrc";
	currentWorkspace = 0;
	loadSettings();

	if(workspaces.empty())
		workspaces.append("/home/kostas/tmp/cheesy-fos");

	refresh();
}

//------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
	stopUI();
	saveSettings();
	delete ui;
}

//------------------------------------------------------------------------------
void MainWindow::on_actionRefresh_triggered()
{
	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionOpen_triggered()
{
	QString path = QFileDialog::getExistingDirectory (this, tr("Fossil Checkout"));
	if(!path.isNull())
	{
		workspaces.append(path);
		currentWorkspace = workspaces.size()-1;
		refresh();
	}
}

//------------------------------------------------------------------------------
static void RecurseDirectory(QFileInfoList &entries, const QString& dirPath, const QString &baseDir)
{
	QDir dir(dirPath);

	QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::Hidden);
	for (int i=0; i<list.count(); ++i)
	{
		QFileInfo info = list[i];

		QString filepath = info.filePath();
		if (info.isDir())
		{
			// recursive
			if (info.fileName()!=".." && info.fileName()!=".")
			{
				RecurseDirectory(entries, filepath, baseDir);
			}
		}
		else
		{
			entries.push_back(info);
		}
	}
}

//------------------------------------------------------------------------------
void MainWindow::refresh()
{
	// Scan all workspace files
	QFileInfoList all_files;
	QString wkdir = getCurrentWorkspace();

	RecurseDirectory(all_files, wkdir, wkdir);
	workspaceFiles.clear();
	for(QFileInfoList::iterator it=all_files.begin(); it!=all_files.end(); ++it)
	{
		if(it->fileName()== "_FOSSIL_")
			continue;

		FileEntry e;
		e.status = FileEntry::STATUS_UNKNOWN;
		e.fileinfo = *it;
		e.filename = e.getRelativeFilename(wkdir);
		workspaceFiles.insert(e.filename, e);
	}

	// Retrieve the status of files tracked by fossil
	QStringList res;
	if(!runFossil(res, QStringList() << "ls" << "-l"))
		return;

	for(QStringList::iterator it=res.begin(); it!=res.end(); ++it)
	{
		QString line = (*it).trimmed();
		if(line.length()==0)
			continue;

		QString status_text = line.left(10).trimmed();
		FileEntry::Status status = FileEntry::STATUS_UNKNOWN;

		if(status_text=="EDITED")
			status = FileEntry::STATUS_EDITTED;
		else if(status_text=="UNCHANGED")
			status = FileEntry::STATUS_UNCHAGED;

		QString fname = line.right(line.length() - 10).trimmed();

		filemap_t::iterator it = workspaceFiles.find(fname);
		Q_ASSERT(it!=workspaceFiles.end());

		it.value().status = status;
	}

	// Update the model
	itemModel.clear();
	itemModel.setHorizontalHeaderLabels(QStringList() << "Status" << "File" << "Ext" );

	size_t i=0;
	for(filemap_t::iterator it = workspaceFiles.begin(); it!=workspaceFiles.end(); ++it, ++i)
	{
		const FileEntry &e = it.value();
		switch(e.status)
		{
			case FileEntry::STATUS_EDITTED:
			{
				QIcon modicon(":icons/icons/Button Blank Yellow-01.png");
				itemModel.setItem(i, 0, new QStandardItem(modicon, "Edited"));
				break;
			}
			case FileEntry::STATUS_UNCHAGED:
			{
				QIcon modicon(":icons/icons/Button Blank Green-01.png");
				itemModel.setItem(i, 0, new QStandardItem(modicon, "Unchanged"));
				break;
			}
			default:
			{
				QIcon modicon(":icons/icons/Button Blank Gray-01.png");
				itemModel.setItem(i, 0, new QStandardItem(modicon, "Unknown"));
			}

		}

		itemModel.setItem(i, 1, new QStandardItem(e.filename));
		itemModel.setItem(i, 2, new QStandardItem( e.fileinfo.completeSuffix()));

	}
	 ui->tableView->resizeColumnsToContents();
	 ui->tableView->resizeRowsToContents();
}

//------------------------------------------------------------------------------
void MainWindow::Log(const QString &text)
{
	ui->textBrowser->append(text);
}
//------------------------------------------------------------------------------
void MainWindow::on_actionClearLog_triggered()
{
	ui->textBrowser->clear();
}

//------------------------------------------------------------------------------
bool MainWindow::runFossil(QStringList &result, const QStringList &args)
{
	QProcess process;

	process.setProcessChannelMode(QProcess::MergedChannels);
	process.setWorkingDirectory(getCurrentWorkspace());

	QStringList rargs;
	rargs << args;

	Log("> fossil "+rargs.join(" "));

	process.start(fossilPath, rargs);
	if(!process.waitForStarted())
	{
		Log(fossilPath + " does not exist\n");
		return false;
	}

	process.waitForFinished();

	QString output = process.readAllStandardOutput();

	QStringList lines = output.split('\n');

	for(QStringList::iterator it=lines.begin(); it!=lines.end(); ++it)
	{
		QString line = it->trimmed();
		result.append(line);
		Log(line);
	}

	return true;
}

//------------------------------------------------------------------------------
void MainWindow::on_tableView_customContextMenuRequested(const QPoint &pos)
{
	QModelIndex idx = ui->tableView->indexAt(pos);
	if(!idx.isValid())
		return;

	QMenu *menu = new QMenu;
	menu->addAction("Diff");
	menu->addSeparator();
	menu->addAction("Add");
	menu->addAction("Delete");
	menu->addSeparator();
	menu->addAction("Commit");
	menu->exec(pos);
}

//------------------------------------------------------------------------------
void MainWindow::loadSettings()
{
	QSettings settings(settingsFile, QSettings::NativeFormat);

	if(settings.contains("FossilPath"))
		fossilPath = settings.value("FossilPath").toString();
	else
		fossilPath = "fossil";

	int num_wks = 0;

	if(settings.contains("NumWorkspaces"))
		num_wks = settings.value("NumWorkspaces").toInt();

	for(int i=0; i<num_wks; ++i)
	{
		QString key = "Workspace_" + QString::number(i);
		QString wk = settings.value(key).toString();
		if(!wk.isEmpty())
			workspaces.append(wk);
	}

	if(settings.contains("LastWorkspace"))
		currentWorkspace = settings.value("LastWorkspace").toInt();
	else
		currentWorkspace = 0;

	if(settings.contains("WindowX") && settings.contains("WindowY"))
	{
		QPoint _pos;
		_pos.setX(settings.value("WindowX").toInt());
		_pos.setY(settings.value("WindowY").toInt());
		move(_pos);
	}

	if(settings.contains("WindowWidth") && settings.contains("WindowHeight"))
	{
		QSize _size;
		_size.setWidth(settings.value("WindowWidth").toInt());
		_size.setHeight(settings.value("WindowHeight").toInt());
		resize(_size);
	}
}

//------------------------------------------------------------------------------
void MainWindow::saveSettings()
{
	QSettings settings(settingsFile, QSettings::NativeFormat);
	settings.setValue("FossilPath", fossilPath);
	settings.setValue("NumWorkspaces", workspaces.size());

	for(int i=0; i<workspaces.size(); ++i)
	{
		QString key = "Workspace_" + QString::number(i);
		settings.setValue(key, workspaces[i]);
	}

	settings.setValue("LastWorkspace", currentWorkspace);
	settings.setValue("WindowX", x());
	settings.setValue("WindowY", y());
	settings.setValue("WindowWidth", width());
	settings.setValue("WindowHeight", height());

}

//------------------------------------------------------------------------------
void MainWindow::getSelectionFilenames(QStringList &filenames)
{
	QModelIndexList selection = ui->tableView->selectionModel()->selectedIndexes();
	for(QModelIndexList::iterator mi_it = selection.begin(); mi_it!=selection.end(); ++mi_it)
	{
		const QModelIndex &mi = *mi_it;

		// FIXME: we are being called once per cell of each row
		// but we only need column 1. There must be a better way
		if(mi.column()!=1)
			continue;

		QVariant data = itemModel.data(mi);
		filenames.append(data.toString());
	}
}
//------------------------------------------------------------------------------
void MainWindow::on_actionDiff_triggered()
{
	QStringList selection;
	getSelectionFilenames(selection);

	for(QStringList::iterator it = selection.begin(); it!=selection.end(); ++it)
	{
		QStringList res;
		if(!runFossil(res, QStringList() << "gdiff" << *it))
			return;
	}
}

//------------------------------------------------------------------------------
bool MainWindow::startUI()
{
	if(uiRunning())
		return true;

	fossilUI.setProcessChannelMode(QProcess::MergedChannels);
	fossilUI.setWorkingDirectory(getCurrentWorkspace());

	Log("> fossil ui");

	fossilUI.start(fossilPath, QStringList() << "ui");
	if(!fossilUI.waitForStarted())
	{
		Log(fossilPath + " does not exist\n");
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------
void MainWindow::stopUI()
{
	if(uiRunning())
		fossilUI.terminate();
}


//------------------------------------------------------------------------------
void MainWindow::on_actionFossilUI_toggled(bool arg1)
{
	if(arg1)
		startUI();
	else
		stopUI();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionQuit_triggered()
{
	close();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionTimeline_triggered()
{
	if(!uiRunning())
		ui->actionFossilUI->activate(QAction::Trigger);

	Q_ASSERT(uiRunning());

	QDesktopServices::openUrl(QUrl("http://localhost:8080/timeline"));
}

//------------------------------------------------------------------------------
void MainWindow::on_actionHistory_triggered()
{
	if(!uiRunning())
		ui->actionFossilUI->activate(QAction::Trigger);

	Q_ASSERT(uiRunning());

	QStringList selection;
	getSelectionFilenames(selection);

	for(QStringList::iterator it = selection.begin(); it!=selection.end(); ++it)
	{
		QDesktopServices::openUrl(QUrl("http://localhost:8080/finfo?name="+*it));
	}
}

