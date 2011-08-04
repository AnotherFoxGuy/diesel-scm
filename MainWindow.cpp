#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QFileDialog>
#include <QStandardItem>
#include <QProcess>
#include <QSettings>
#include <QDesktopServices>
#include <QDateTime>
#include <QLabel>
#include <QTemporaryFile>
#include <QMessageBox>
#include <QUrl>
#include "CommitDialog.h"
#include "FileActionDialog.h"

#define SILENT_STATUS true
#define COUNTOF(array) (sizeof(array)/sizeof(array[0]))

#define DEV_SETTINGS



enum
{
	COLUMN_STATUS,
	COLUMN_PATH,
	COLUMN_FILENAME,
	COLUMN_EXTENSION,
	COLUMN_MODIFIED
};

static QString QuotePath(const QString &path)
{
	return path;
}

static QStringList QuotePaths(const QStringList &paths)
{
	QStringList res;
	for(int i=0; i<paths.size(); ++i)
		res.append(QuotePath(paths[i]));
	return res;
}



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
	ui->tableView->setModel(&itemModel);
	itemModel.setHorizontalHeaderLabels(QStringList() << tr("S") << tr("Path") << tr("File") << tr("Ext") << tr("Modified") );

	ui->tableView->addAction(ui->actionDiff);
	ui->tableView->addAction(ui->actionHistory);
	ui->tableView->addAction(ui->actionOpenFile);
	ui->tableView->addAction(ui->actionOpenContaining);
	ui->tableView->addAction(ui->actionAdd);
	ui->tableView->addAction(ui->actionDelete);
	ui->tableView->addAction(ui->actionRename);

	statusLabel = new QLabel();
	statusLabel->setMinimumSize( statusLabel->sizeHint() );
	ui->statusBar->addWidget( statusLabel, 1 );

	settingsFile = QDir::homePath() + QDir::separator() + ".fuelrc";
	currentWorkspace = 0;

#ifdef DEV_SETTINGS
	if(workspaces.empty())
		workspaces.append("/home/kostas/tmp/testfossil");

	fossilPath = "fossil";
#else
	loadSettings();
#endif

	refresh();
}

//------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
	stopUI();
#ifndef DEV_SETTINGS
	saveSettings();
#endif
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
	QString path = QFileDialog::getExistingDirectory(this, tr("Fossil Checkout"));
	if(!path.isNull())
	{
		workspaces.append(path);
		currentWorkspace = workspaces.size()-1;
		on_actionClearLog_triggered();
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
void MainWindow::enableActions(bool on)
{
	ui->actionCommit->setEnabled(on);
	ui->actionDiff->setEnabled(on);
	ui->actionAdd->setEnabled(on);
	ui->actionDelete->setEnabled(on);
	ui->actionPush->setEnabled(on);
	ui->actionPull->setEnabled(on);
	ui->actionRename->setEnabled(on);
	ui->actionHistory->setEnabled(on);
	ui->actionFossilUI->setEnabled(on);
	ui->actionRevert->setEnabled(on);
	ui->actionTimeline->setEnabled(on);
	ui->actionOpenFile->setEnabled(on);
	ui->actionOpenContaining->setEnabled(on);
}
//------------------------------------------------------------------------------
void MainWindow::refresh()
{
	// Load repository info
	RepoStatus st = getRepoStatus();

	if(st==REPO_NOT_FOUND)
	{
		setStatus(tr("No checkout detected."));
		enableActions(false);
		itemModel.removeRows(0, itemModel.rowCount());
		return;
	}
	else if(st==REPO_OLD_SCHEMA)
	{
		setStatus(tr("Old fossil schema detected. Consider running rebuild."));
		enableActions(false);
		itemModel.removeRows(0, itemModel.rowCount());
		return;
	}

	scanWorkspace();
	setStatus("");
	enableActions(true);

	QString title = "Fuel";
	if(!projectName.isEmpty())
		title += " - "+projectName;

	setWindowTitle(title);
}

//------------------------------------------------------------------------------
void MainWindow::scanWorkspace()
{
	// Scan all workspace files
	QFileInfoList all_files;
	QString wkdir = getCurrentWorkspace();

	RecurseDirectory(all_files, wkdir, wkdir);
	workspaceFiles.clear();
	for(QFileInfoList::iterator it=all_files.begin(); it!=all_files.end(); ++it)
	{
		QString filename = it->fileName();

		// Skip fossil files
		if(filename == "_FOSSIL_" || (!repositoryFile.isEmpty() && it->absoluteFilePath()==repositoryFile))
			continue;

		FileEntry e;
		e.set(*it, FileEntry::TYPE_UNKNOWN, wkdir);
		workspaceFiles.insert(e.getFilename(), e);
	}

	// Retrieve the status of files tracked by fossil
	QStringList res;
	if(!runFossil(res, QStringList() << "ls" << "-l", SILENT_STATUS))
		return;

	for(QStringList::iterator it=res.begin(); it!=res.end(); ++it)
	{
		QString line = (*it).trimmed();
		if(line.length()==0)
			continue;

		QString status_text = line.left(10).trimmed();
		FileEntry::EntryType type = FileEntry::TYPE_UNKNOWN;

		if(status_text=="EDITED")
			type = FileEntry::TYPE_EDITTED;
		if(status_text=="ADDED")
			type = FileEntry::TYPE_ADDED;
		if(status_text=="DELETED")
			type = FileEntry::TYPE_DELETED;
		else if(status_text=="UNCHANGED")
			type = FileEntry::TYPE_UNCHANGED;

		QString fname = line.right(line.length() - 10).trimmed();

		filemap_t::iterator it = workspaceFiles.find(fname);
		Q_ASSERT(it!=workspaceFiles.end());

		it.value().setType(type);
	}

	// Update the model
	// Clear all rows (except header)
	itemModel.removeRows(0, itemModel.rowCount());

	struct { FileEntry::EntryType type; const char *tag; const char *icon; }
	stats[]=
	{
		{	FileEntry::TYPE_EDITTED, "E", ":icons/icons/Button Blank Yellow-01.png" },
		{	FileEntry::TYPE_UNCHANGED, "U", ":icons/icons/Button Blank Green-01.png" },
		{	FileEntry::TYPE_ADDED, "A", ":icons/icons/Button Add-01.png" },
		{	FileEntry::TYPE_DELETED, "D", ":icons/icons/Button Close-01.png" },
	};

	size_t i=0;
	for(filemap_t::iterator it = workspaceFiles.begin(); it!=workspaceFiles.end(); ++it, ++i)
	{
		const FileEntry &e = it.value();

		// Status Column
		const char *tag = "?"; // Default Tag
		const char *icon = ":icons/icons/Button Blank Gray-01.png"; // Default icon

		for(size_t t=0; t<COUNTOF(stats); ++t)
		{
			if(e.getType() == stats[t].type)
			{
				tag = stats[t].tag;
				icon = stats[t].icon;
				break;
			}
		}

		itemModel.setItem(i, COLUMN_STATUS, new QStandardItem(QIcon(icon), tag));

		QString path = e.getFilename();
		path = path.left(path.indexOf(e.getFileInfo().fileName()));
		QFileInfo finfo = e.getFileInfo();

		itemModel.setItem(i, COLUMN_PATH, new QStandardItem(path));
		itemModel.setItem(i, COLUMN_FILENAME, new QStandardItem(e.getFilename()));
		itemModel.setItem(i, COLUMN_EXTENSION, new QStandardItem(finfo .completeSuffix()));
		itemModel.setItem(i, COLUMN_MODIFIED, new QStandardItem(finfo .lastModified().toString(Qt::SystemLocaleShortDate)));

	}

	ui->tableView->resizeColumnsToContents();
	ui->tableView->resizeRowsToContents();
}
//------------------------------------------------------------------------------
MainWindow::RepoStatus MainWindow::getRepoStatus()
{
	QStringList res;
	int exit_code = EXIT_FAILURE;

	// We need to differentiate the reason why fossil has failed
	// so we delay processing of the exit_code
	if(!runFossil(res, QStringList() << "info", exit_code, SILENT_STATUS))
		return REPO_NOT_FOUND;

	bool run_ok = exit_code == EXIT_SUCCESS;

	for(QStringList::iterator it=res.begin(); it!=res.end(); ++it)
	{
		QStringList tokens = it->split(":");
		if(tokens.length()!=2)
			continue;
		QString key = tokens[0].trimmed();
		QString value = tokens[1].trimmed();

		if(key=="fossil")
		{
			if(value=="incorrect repository schema version")
				return REPO_OLD_SCHEMA;
			else if(value=="not within an open checkout")
				return REPO_NOT_FOUND;
		}

		if(run_ok)
		{
			if(key=="project-name")
				projectName = value;
			else if(key=="repository")
				repositoryFile = value;
		}
	}

	return run_ok ? REPO_OK : REPO_NOT_FOUND;
}
//------------------------------------------------------------------------------
void MainWindow::log(const QString &text)
{
	ui->textBrowser->insertPlainText(text);
	QTextCursor c = ui->textBrowser->textCursor();
	c.movePosition(QTextCursor::End);
	ui->textBrowser->setTextCursor(c);
}

//------------------------------------------------------------------------------
void MainWindow::setStatus(const QString &text)
{
	Q_ASSERT(statusLabel);
	statusLabel->setText(text);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionClearLog_triggered()
{
	ui->textBrowser->clear();
}

//------------------------------------------------------------------------------
bool MainWindow::runFossil(QStringList &result, const QStringList &args, bool silent)
{
	int exit_code = EXIT_FAILURE;
	if(!runFossil(result, args, exit_code, silent))
		return false;

	return exit_code == EXIT_SUCCESS;
}
//------------------------------------------------------------------------------
// Run fossil. Returns true if execution was succesfull regardless if fossil
// issued an error
bool MainWindow::runFossil(QStringList &result, const QStringList &args, int &exitCode, bool silent)
{
	QProcess process(this);

	process.setProcessChannelMode(QProcess::MergedChannels);
	QString wkdir = getCurrentWorkspace();
	process.setWorkingDirectory(wkdir);

	if(!silent)
		log("> fossil "+args.join(" ")+"\n");

	process.start(fossilPath, args);
	if(!process.waitForStarted())
	{
		log("Could not start fossil executable '"+fossilPath + "''\n");
		return false;
	}

	process.waitForFinished();
	QString output = process.readAllStandardOutput();

	QStringList lines = output.split('\n');

	for(QStringList::iterator it=lines.begin(); it!=lines.end(); ++it)
	{
		QString line = it->trimmed();
		result.append(line);
		if(!silent)
			log(line+"\n");
	}

	QProcess::ExitStatus es = process.exitStatus();

	if(es!=QProcess::NormalExit)
		return false;

	exitCode = process.exitCode();
	return true;
}

//------------------------------------------------------------------------------
void MainWindow::addWorkspace(const QString &dir)
{
	workspaces.append(dir);
	currentWorkspace = workspaces.size()-1;
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
void MainWindow::getSelectionFilenames(QStringList &filenames, int includeMask, bool allIfEmpty)
{
	QModelIndexList selection = ui->tableView->selectionModel()->selectedIndexes();
	if(selection.empty() && allIfEmpty)
	{
		ui->tableView->selectAll();
		selection = ui->tableView->selectionModel()->selectedIndexes();
		ui->tableView->clearSelection();
	}

	for(QModelIndexList::iterator mi_it = selection.begin(); mi_it!=selection.end(); ++mi_it)
	{
		const QModelIndex &mi = *mi_it;

		// FIXME: we are being called once per cell of each row
		// but we only need column 1. There must be a better way
		if(mi.column()!=COLUMN_FILENAME)
			continue;

		QVariant data = itemModel.data(mi);
		QString filename = data.toString();
		filemap_t::iterator e_it = workspaceFiles.find(filename);
		Q_ASSERT(e_it!=workspaceFiles.end());
		const FileEntry &e = e_it.value();

		// Skip unwanted files
		if(!(includeMask & e.getType()))
			continue;

		filenames.append(filename);
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
		if(!runFossil(res, QStringList() << "gdiff" << QuotePath(*it)))
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

	log("> fossil ui\n");

	fossilUI.start(fossilPath, QStringList() << "ui");
	if(!fossilUI.waitForStarted())
	{
		log(fossilPath + tr(" does not exist") +"\n");
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

//------------------------------------------------------------------------------
void MainWindow::on_tableView_doubleClicked(const QModelIndex &/*index*/)
{
	on_actionDiff_triggered();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionOpenFile_triggered()
{
	QStringList selection;
	getSelectionFilenames(selection);

	for(QStringList::iterator it = selection.begin(); it!=selection.end(); ++it)
	{
		QDesktopServices::openUrl(QUrl::fromLocalFile(getCurrentWorkspace()+QDir::separator()+*it));
	}
}

//------------------------------------------------------------------------------
void MainWindow::on_actionPush_triggered()
{
	QStringList res;
	runFossil(res, QStringList() << "push");
}

//------------------------------------------------------------------------------
void MainWindow::on_actionPull_triggered()
{
	QStringList res;
	runFossil(res, QStringList() << "pull");
}


//------------------------------------------------------------------------------
void MainWindow::on_actionCommit_triggered()
{
	QStringList modified_files;
	getSelectionFilenames(modified_files, FileEntry::TYPE_REPO_MODIFIED, true);

	if(modified_files.empty())
		return;

	QString msg;
	if(!CommitDialog::run(msg, commitMessages, modified_files, this))
		return;

	// Do commit
	commitMessages.push_front(msg);

	{
		QTemporaryFile comment_file;
		comment_file.open();
		comment_file.write(msg.toUtf8());
		comment_file.close();

		QStringList res;
		runFossil(res, QStringList() << "commit" << "--message-file" << QuotePath(comment_file.fileName()) << QuotePaths(modified_files) );
	}

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionAdd_triggered()
{
	// Get unknown files only
	QStringList selection;
	getSelectionFilenames(selection, FileEntry::TYPE_UNKNOWN);

	if(selection.empty())
		return;

	if(!FileActionDialog::run(tr("Add files"), tr("The following files will be added. Are you sure?"), selection, this))
		return;

	// Do Add
	QStringList res;
	runFossil(res, QStringList() << "add" << QuotePaths(selection) );

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionDelete_triggered()
{
	QStringList repo_files;
	getSelectionFilenames(repo_files, FileEntry::TYPE_REPO);

	QStringList unknown_files;
	getSelectionFilenames(unknown_files, FileEntry::TYPE_UNKNOWN);

	if(repo_files.empty() && unknown_files.empty())
		return;

	if(!FileActionDialog::run(tr("Delete files"), tr("The following files will be deleted. Are you sure?"), repo_files+unknown_files, this))
		return;

	// Do Delete
	QStringList res;
	runFossil(res, QStringList() << "delete" << QuotePaths(repo_files) );

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionRevert_triggered()
{
	QStringList modified_files;
	getSelectionFilenames(modified_files, FileEntry::TYPE_ADDED|FileEntry::TYPE_EDITTED);

	if(modified_files.empty())
		return;

	if(!FileActionDialog::run(tr("Revert files"), tr("The following files will be reverted. Are you sure?"), modified_files, this))
		return;

	// Do Revert
	QStringList res;
	runFossil(res, QStringList() << "revert" << QuotePaths(modified_files) );

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionNew_triggered()
{
	QString filter(tr("Fossil Repositories (*.fossil)"));

	QString path = QFileDialog::getSaveFileName(
				this,
				tr("New Fossil Repository"),
				QString(),
				filter,
				&filter);

	if(path.isEmpty())
		return;

	if(QFile::exists(path))
	{
		QMessageBox::critical(this, tr("Error"), tr("A repository file already exists.\nRepository creation aborted."), QMessageBox::Ok );
		return;
	}

	QFileInfo path_info(path);
	Q_ASSERT(path_info.dir().exists());
	QString wkdir = path_info.absoluteDir().absolutePath();
	addWorkspace(wkdir);
	repositoryFile = path_info.absoluteFilePath();

	// Create repo
	QStringList res;
	if(!runFossil(res, QStringList() << "new" << QuotePath(repositoryFile), false))
	{
		QMessageBox::critical(this, tr("Error"), tr("Repository creation failed."), QMessageBox::Ok );
		return;
	}

	// Open repo
	if(!runFossil(res, QStringList() << "open" << QuotePath(repositoryFile), false))
	{
		QMessageBox::critical(this, tr("Error"), tr("Repository checkout failed."), QMessageBox::Ok );
		return;
	}

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionClone_triggered()
{

}


//------------------------------------------------------------------------------
void MainWindow::on_actionOpenContaining_triggered()
{
	QStringList selection;
	getSelectionFilenames(selection);

	QString target;

	if(selection.empty())
		target = QDir::toNativeSeparators(getCurrentWorkspace());
	else
	{
		QFileInfo file_info(selection[0]);
		target = QDir::toNativeSeparators(file_info.absoluteDir().absolutePath());
	}

	QUrl url = QUrl::fromLocalFile(target);
	QDesktopServices::openUrl(url);
}
