#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QFileDialog>
#include <QStandardItem>
#include <QProcess>
#include <QSettings>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
	ui->tableView->setModel(&itemModel);
	ui->tableView->addAction(ui->actionAdd);
	ui->tableView->addAction(ui->actionDelete);
	ui->tableView->addAction(ui->actionRename);
	ui->tableView->addAction(ui->actionHistory);
	ui->tableView->addAction(ui->actionDiff);

	settingsFile = QApplication::applicationDirPath().left(1) + ":/qfossil.ini";
	currentWorkspace = 0;
	loadSettings();

	if(workspaces.empty())
		workspaces.append("/home/kostas/tmp/cheesy-fos");

	if(fossilPath.isEmpty())
		fossilPath = "fossil";

//	fossilPath = "/home/kostas/local/bin/fossil";
//	repoPath = ;
	refresh();
}

MainWindow::~MainWindow()
{
	saveSettings();
	delete ui;
}

void MainWindow::on_actionRefresh_triggered()
{
	refresh();
}

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

void RecurseDirectory(QFileInfoList &entries, const QString& dirPath, const QString &baseDir)
{
	QDir dir(dirPath);

	QFileInfoList list = dir.entryInfoList(QDir::AllEntries);
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

void MainWindow::refresh()
{
	QStringList res;
	if(!runFossil(res, QStringList() << "ls" << "-l"))
		return;

	// Scan all files
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
				QIcon modicon(":icons/icons/Button-Blank-Yellow-icon.png");
				itemModel.setItem(i, 0, new QStandardItem(modicon, "Edited"));
				break;
			}
			case FileEntry::STATUS_UNCHAGED:
			{
				QIcon modicon(":icons/icons/Button-Blank-Green-icon.png");
				itemModel.setItem(i, 0, new QStandardItem(modicon, "Unchanged"));
				break;
			}
			default:
			{
				QIcon modicon(":icons/icons/Button-Blank-Gray-icon.png");
				itemModel.setItem(i, 0, new QStandardItem(modicon, "Unknown"));
			}

		}

		itemModel.setItem(i, 1, new QStandardItem(e.filename));
		itemModel.setItem(i, 2, new QStandardItem( e.fileinfo.completeSuffix()));

	}
	 ui->tableView->resizeColumnsToContents();
	 ui->tableView->resizeRowsToContents();
}

bool MainWindow::runFossil(QStringList &result, const QStringList &args)
{
	QProcess process;

	process.setProcessChannelMode(QProcess::MergedChannels);
	process.setWorkingDirectory(getCurrentWorkspace());

	QStringList rargs;
	rargs << args;

	ui->textBrowser->append("fossil "+rargs.join(" "));

	process.start(fossilPath, rargs);
	if(!process.waitForStarted())
	{
		ui->textBrowser->append(fossilPath + " does not exist\n");
		return false;
	}

	process.waitForFinished();

	QString output = process.readAllStandardOutput();

	QStringList lines = output.split('\n');

	for(QStringList::iterator it=lines.begin(); it!=lines.end(); ++it)
	{
		QString &line = *it;
		result.append(line.trimmed());
		ui->textBrowser->append( line.trimmed());
	}

	return true;
}

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

	//	QMen
}

void MainWindow::loadSettings()
{
	QSettings settings(settingsFile, QSettings::NativeFormat);

	bool ok=false;

	fossilPath = settings.value("FossilPath").toString();
	if(fossilPath.isEmpty())
		fossilPath = "fossil";

	int num_repos =	settings.value("NumWorkspaces").toInt(&ok);
	if(!ok)
		num_repos=0;

	for(int i=0; i<num_repos; ++i)
	{
	  QString wk = settings.value("Workspace_"+i).toString();
	  if(!wk.isEmpty())
			workspaces.append(wk);
	}

	currentWorkspace = settings.value("LastWorkspace").toInt(&ok);
	if(!ok)
		num_repos=0;

}

void MainWindow::saveSettings()
{
	QSettings settings(settingsFile, QSettings::NativeFormat);
	settings.setValue("FossilPath", fossilPath);
	settings.setValue("NumWorkspaces", workspaces.size());

	for(size_t i=0; i<workspaces.size(); ++i)
		settings.setValue("Workspace_"+i, workspaces[i]);

	settings.setValue("LastWorkspace", currentWorkspace);
}
