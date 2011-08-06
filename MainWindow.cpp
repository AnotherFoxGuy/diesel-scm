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
#include <QInputDialog>
#include "CommitDialog.h"
#include "FileActionDialog.h"

#define SILENT_STATUS true
#define COUNTOF(array) (sizeof(array)/sizeof(array[0]))

//#define DEV_SETTINGS

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

	// Locate a sequence of two separator actions in file menu
	QList<QAction*> file_actions = ui->menuFile->actions();
	QAction *recent_sep=0;
	for(int i=0; i<file_actions.size(); ++i)
	{
		QAction *act = file_actions[i];
		if(act->isSeparator() && i>0 && file_actions[i-1]->isSeparator())
		{
			recent_sep = act;
			break;
		}
	}
	Q_ASSERT(recent_sep);
	for (int i = 0; i < MAX_RECENT; ++i)
	{
		recentWorkspaceActs[i] = new QAction(this);
		recentWorkspaceActs[i]->setVisible(false);
		connect(recentWorkspaceActs[i], SIGNAL(triggered()), this, SLOT(onOpenRecent()));
		ui->menuFile->insertAction(recent_sep, recentWorkspaceActs[i]);
	}

	statusLabel = new QLabel();
	statusLabel->setMinimumSize( statusLabel->sizeHint() );
	ui->statusBar->addWidget( statusLabel, 1 );

#ifdef DEV_SETTINGS
	currentWorkspace = "/home/kostas/tmp/testfossil";
	fossilPath = "fossil";
#else
	loadSettings();
#endif

	refresh();
	rebuildRecent();
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
const QString &MainWindow::getCurrentWorkspace()
{
	return currentWorkspace;
}

//------------------------------------------------------------------------------
void MainWindow::on_actionRefresh_triggered()
{
	refresh();
}

//------------------------------------------------------------------------------
bool MainWindow::openWorkspace(const QString &dir)
{
	addWorkspace(dir);
	currentWorkspace = dir;

	on_actionClearLog_triggered();
	stopUI();

	bool ok = refresh();
	if(ok)
	{
		QDir::setCurrent(dir);
		rebuildRecent();
	}

	return ok;
}

//------------------------------------------------------------------------------
void MainWindow::on_actionOpen_triggered()
{
	QString path = QFileDialog::getExistingDirectory(this, tr("Fossil Checkout"));
	if(!path.isNull())
		openWorkspace(path);
}

//------------------------------------------------------------------------------
void MainWindow::rebuildRecent()
{
	for(int i = 0; i < MAX_RECENT; ++i)
		recentWorkspaceActs[i]->setVisible(false);

	int enabled_acts = qMin<int>(MAX_RECENT, workspaceHistory.size());

	for(int i = 0; i < enabled_acts; ++i)
	{
		QString text = tr("&%1 %2").arg(i + 1).arg(workspaceHistory[i]);

		recentWorkspaceActs[i]->setText(text);
		recentWorkspaceActs[i]->setData(workspaceHistory[i]);
		recentWorkspaceActs[i]->setVisible(true);
	}

}

//------------------------------------------------------------------------------
void MainWindow::onOpenRecent()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (action)
		openWorkspace(action->data().toString());
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
	ui->actionUndo->setEnabled(on);
	ui->actionUpdate->setEnabled(on);
}
//------------------------------------------------------------------------------
bool MainWindow::refresh()
{
	// Load repository info
	RepoStatus st = getRepoStatus();

	if(st==REPO_NOT_FOUND)
	{
		setStatus(tr("No checkout detected."));
		enableActions(false);
		itemModel.removeRows(0, itemModel.rowCount());
		return false;
	}
	else if(st==REPO_OLD_SCHEMA)
	{
		setStatus(tr("Old fossil schema detected. Consider running rebuild."));
		enableActions(false);
		itemModel.removeRows(0, itemModel.rowCount());
		return true;
	}

	scanWorkspace();
	setStatus("");
	enableActions(true);

	QString title = "Fuel";
	if(!projectName.isEmpty())
		title += " - "+projectName;

	setWindowTitle(title);
	return true;
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

		RepoFile e;
		e.set(*it, RepoFile::TYPE_UNKNOWN, wkdir);
		workspaceFiles.insert(e.getFilename(), e);
	}

	// Retrieve the status of files tracked by fossil
	QStringList res;
	if(!runFossil(QStringList() << "ls" << "-l", &res, SILENT_STATUS))
		return;

	for(QStringList::iterator it=res.begin(); it!=res.end(); ++it)
	{
		QString line = (*it).trimmed();
		if(line.length()==0)
			continue;

		QString status_text = line.left(10).trimmed();
		QString fname = line.right(line.length() - 10).trimmed();
		RepoFile::EntryType type = RepoFile::TYPE_UNKNOWN;

		bool add_missing = false;

		if(status_text=="EDITED")
			type = RepoFile::TYPE_EDITTED;
		else if(status_text=="ADDED")
			type = RepoFile::TYPE_ADDED;
		else if(status_text=="DELETED")
		{
			type = RepoFile::TYPE_DELETED;
			add_missing = true;
		}
		else if(status_text=="MISSING")
		{
			type = RepoFile::TYPE_MISSING;
			add_missing = true;
		}
		else if(status_text=="RENAMED")
			type = RepoFile::TYPE_RENAMED;
		else if(status_text=="UNCHANGED")
			type = RepoFile::TYPE_UNCHANGED;

		filemap_t::iterator it = workspaceFiles.find(fname);

		if(add_missing && it==workspaceFiles.end())
		{
			RepoFile e;
			QFileInfo info(wkdir+QDir::separator()+fname);
			e.set(info, type, wkdir);
			workspaceFiles.insert(e.getFilename(), e);
		}

		it = workspaceFiles.find(fname);
		Q_ASSERT(it!=workspaceFiles.end());

		it.value().setType(type);
	}

	// Update the model
	// Clear all rows (except header)
	itemModel.removeRows(0, itemModel.rowCount());

	struct { RepoFile::EntryType type; const char *tag; const char *tooltip; const char *icon; }
	stats[] = {
		{	RepoFile::TYPE_EDITTED, "E", "Editted", ":icons/icons/Button Blank Yellow-01.png" },
		{	RepoFile::TYPE_UNCHANGED, "U", "Unchanged", ":icons/icons/Button Blank Green-01.png" },
		{	RepoFile::TYPE_ADDED, "A", "Added", ":icons/icons/Button Add-01.png" },
		{	RepoFile::TYPE_DELETED, "D", "Deleted", ":icons/icons/Button Close-01.png" },
		{	RepoFile::TYPE_RENAMED, "R", "Renamed", ":icons/icons/Button Reload-01.png" },
		{	RepoFile::TYPE_MISSING, "M", "Missing", ":icons/icons/Button Help-01.png" },
	};

	size_t i=0;
	for(filemap_t::iterator it = workspaceFiles.begin(); it!=workspaceFiles.end(); ++it, ++i)
	{
		const RepoFile &e = it.value();

		// Status Column
		const char *tag = "?"; // Default Tag
		const char *tooltip = "Unknown";
		const char *icon = ":icons/icons/Button Blank Gray-01.png"; // Default icon

		for(size_t t=0; t<COUNTOF(stats); ++t)
		{
			if(e.getType() == stats[t].type)
			{
				tag = stats[t].tag;
				tooltip = stats[t].tooltip;
				icon = stats[t].icon;
				break;
			}
		}

		QStandardItem *status = new QStandardItem(QIcon(icon), "");
		status->setToolTip(tooltip);
		itemModel.setItem(i, COLUMN_STATUS, status);

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
	if(!runFossilRaw(QStringList() << "info", &res, &exit_code, SILENT_STATUS))
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
bool MainWindow::runFossil(const QStringList &args, QStringList *output, bool silent, bool detached)
{
	int exit_code = EXIT_FAILURE;
	if(!runFossilRaw(args, output, &exit_code, silent, detached))
		return false;

	return exit_code == EXIT_SUCCESS;
}
//------------------------------------------------------------------------------
// Run fossil. Returns true if execution was succesfull regardless if fossil
// issued an error
bool MainWindow::runFossilRaw(const QStringList &args, QStringList *output, int *exitCode, bool silent, bool detached)
{
	if(!silent)
		log("> fossil "+args.join(" ")+"\n");

	QString wkdir = getCurrentWorkspace();

	if(detached)
	{
		return QProcess::startDetached(settings.fossilPath, args, wkdir);
	}

	QProcess process(this);
	process.setProcessChannelMode(QProcess::MergedChannels);
	process.setWorkingDirectory(wkdir);

	process.start(settings.fossilPath, args);
	if(!process.waitForStarted())
	{
		log("Could not start fossil executable '"+settings.fossilPath + "''\n");
		return false;
	}

	process.waitForFinished();
	QString std_output = process.readAllStandardOutput();

	QStringList lines = std_output.split('\n');

	for(QStringList::iterator it=lines.begin(); it!=lines.end(); ++it)
	{
		QString line = it->trimmed();
		if(line.isEmpty())
			continue;
		if(output)
			output->append(line);
		if(!silent)
			log(line+"\n");
	}

	QProcess::ExitStatus es = process.exitStatus();

	if(es!=QProcess::NormalExit)
		return false;

	if(exitCode)
		*exitCode = process.exitCode();

	return true;
}

//------------------------------------------------------------------------------
void MainWindow::addWorkspace(const QString &dir)
{
	QDir d(dir);
	QString new_workspace = QDir(dir).absolutePath();

	// Do not add the workspace if it exists already
	if(workspaceHistory.indexOf(new_workspace)!=-1)
		return;

	workspaceHistory.append(new_workspace);
}
//------------------------------------------------------------------------------
void MainWindow::loadSettings()
{
	QSettings qsettings(QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());

	if(qsettings.contains("FossilPath"))
		settings.fossilPath = qsettings.value("FossilPath").toString();
	else
		settings.fossilPath = "fossil";

	int num_wks = 0;

	if(qsettings.contains("NumWorkspaces"))
		num_wks = qsettings.value("NumWorkspaces").toInt();

	for(int i=0; i<num_wks; ++i)
	{
		QString key = "Workspace_" + QString::number(i);
		QString wk = qsettings.value(key).toString();
		if(!wk.isEmpty())
			addWorkspace(wk);
	}

	int curr_wkspace = -1;
	if(qsettings.contains("LastWorkspace"))
		curr_wkspace = qsettings.value("LastWorkspace").toInt();

	if(curr_wkspace!=-1 && curr_wkspace< workspaceHistory.size())
		currentWorkspace = workspaceHistory[curr_wkspace];

	if(qsettings.contains("WindowX") && qsettings.contains("WindowY"))
	{
		QPoint _pos;
		_pos.setX(qsettings.value("WindowX").toInt());
		_pos.setY(qsettings.value("WindowY").toInt());
		move(_pos);
	}

	if(qsettings.contains("WindowWidth") && qsettings.contains("WindowHeight"))
	{
		QSize _size;
		_size.setWidth(qsettings.value("WindowWidth").toInt());
		_size.setHeight(qsettings.value("WindowHeight").toInt());
		resize(_size);
	}
}

//------------------------------------------------------------------------------
void MainWindow::saveSettings()
{
	QSettings qsettings(QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
	qsettings.setValue("FossilPath", settings.fossilPath);
	qsettings.setValue("NumWorkspaces", workspaceHistory.size());

	for(int i=0; i<workspaceHistory.size(); ++i)
	{
		QString key = "Workspace_" + QString::number(i);
		qsettings.setValue(key, workspaceHistory[i]);
	}

	int curr_wkspace = workspaceHistory.indexOf(currentWorkspace);
	if(curr_wkspace>-1)
		qsettings.setValue("LastWorkspace", curr_wkspace);

	qsettings.setValue("WindowX", x());
	qsettings.setValue("WindowY", y());
	qsettings.setValue("WindowWidth", width());
	qsettings.setValue("WindowHeight", height());

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
		const RepoFile &e = e_it.value();

		// Skip unwanted files
		if(!(includeMask & e.getType()))
			continue;

		filenames.append(filename);
	}
}
//------------------------------------------------------------------------------
bool MainWindow::diffFile(QString repoFile)
{
	// Run the diff detached
	return runFossil(QStringList() << "gdiff" << QuotePath(repoFile), 0, false, true);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionDiff_triggered()
{
	QStringList selection;
	getSelectionFilenames(selection, RepoFile::TYPE_REPO);

	for(QStringList::iterator it = selection.begin(); it!=selection.end(); ++it)
		if(!diffFile(*it))
			return;
}

//------------------------------------------------------------------------------
bool MainWindow::startUI()
{
	if(uiRunning())
		return true;

	fossilUI.setProcessChannelMode(QProcess::MergedChannels);
	fossilUI.setWorkingDirectory(getCurrentWorkspace());

	log("> fossil ui\n");

	fossilUI.start(settings.fossilPath, QStringList() << "ui");
	if(!fossilUI.waitForStarted())
	{
		log(settings.fossilPath + tr(" does not exist") +"\n");
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
	runFossil(QStringList() << "push");
}

//------------------------------------------------------------------------------
void MainWindow::on_actionPull_triggered()
{
	runFossil(QStringList() << "pull");
}


//------------------------------------------------------------------------------
void MainWindow::on_actionCommit_triggered()
{
	QStringList modified_files;
	getSelectionFilenames(modified_files, RepoFile::TYPE_REPO_MODIFIED, true);

	if(modified_files.empty())
		return;

	QString msg;
	if(!CommitDialog::run(this, msg, commitMessages, modified_files))
		return;

	// Do commit
	commitMessages.push_front(msg);

	{
		QTemporaryFile comment_file;
		comment_file.open();
		comment_file.write(msg.toUtf8());
		comment_file.close();

		runFossil(QStringList() << "commit" << "--message-file" << QuotePath(comment_file.fileName()) << QuotePaths(modified_files) );
	}

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionAdd_triggered()
{
	// Get unknown files only
	QStringList selection;
	getSelectionFilenames(selection, RepoFile::TYPE_UNKNOWN);

	if(selection.empty())
		return;

	if(!FileActionDialog::run(this, tr("Add files"), tr("The following files will be added. Are you sure?"), selection))
		return;

	// Do Add
	runFossil(QStringList() << "add" << QuotePaths(selection) );

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionDelete_triggered()
{
	QStringList repo_files;
	getSelectionFilenames(repo_files, RepoFile::TYPE_REPO);

	QStringList unknown_files;
	getSelectionFilenames(unknown_files, RepoFile::TYPE_UNKNOWN);

	QStringList all_files = repo_files+unknown_files;

	if(all_files.empty())
		return;

	bool remove_local = false;

	if(!FileActionDialog::run(this, tr("Remove files"), tr("The following files will be removed from the repository.\nAre you sure?"), all_files, tr("Also delete the local files"), &remove_local ))
		return;

	if(!repo_files.empty())
	{
		// Do Delete
		if(!runFossil(QStringList() << "delete" << QuotePaths(repo_files)))
			return;
	}

	if(remove_local)
	{
		for(int i=0; i<all_files.size(); ++i)
		{
			QFileInfo fi(getCurrentWorkspace() + QDir::separator() + all_files[i]);
			Q_ASSERT(fi.exists());
			QFile::remove(fi.filePath());
		}
	}

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionRevert_triggered()
{
	QStringList modified_files;
	getSelectionFilenames(modified_files, RepoFile::TYPE_EDITTED|RepoFile::TYPE_DELETED|RepoFile::TYPE_MISSING);

	if(modified_files.empty())
		return;

	if(!FileActionDialog::run(this, tr("Revert files"), tr("The following files will be reverted. Are you sure?"), modified_files))
		return;

	// Do Revert
	runFossil(QStringList() << "revert" << QuotePaths(modified_files) );

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionRename_triggered()
{
	QStringList repo_files;
	getSelectionFilenames(repo_files, RepoFile::TYPE_REPO);

	if(repo_files.length()!=1)
		return;

	QFileInfo fi_before(repo_files[0]);

	bool ok = false;
	QString new_name = QInputDialog::getText(this, tr("Rename"), tr("Enter new name"), QLineEdit::Normal, fi_before.filePath(), &ok );
	if(!ok)
		return;

	QFileInfo fi_after(new_name);

	if(fi_after.exists())
	{
		QMessageBox::critical(this, tr("Error"), tr("File ")+new_name+tr(" already exists.\nRename aborted."), QMessageBox::Ok );
		return;
	}

	// Do Rename
	runFossil(QStringList() << "mv" << QuotePath(fi_before.filePath()) << QuotePath(fi_after.filePath()) );

	QString wkdir = getCurrentWorkspace() + QDir::separator();

	// Also rename the file
	QFile::rename( wkdir+fi_before.filePath(), wkdir+fi_after.filePath());

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
	stopUI();

	QFileInfo path_info(path);
	Q_ASSERT(path_info.dir().exists());
	QString wkdir = path_info.absoluteDir().absolutePath();
	addWorkspace(wkdir);
	repositoryFile = path_info.absoluteFilePath();

	// Create repo
	if(!runFossil(QStringList() << "new" << QuotePath(repositoryFile), 0, false))
	{
		QMessageBox::critical(this, tr("Error"), tr("Repository creation failed."), QMessageBox::Ok );
		return;
	}

	// Open repo
	if(!runFossil(QStringList() << "open" << QuotePath(repositoryFile), 0, false))
	{
		QMessageBox::critical(this, tr("Error"), tr("Repository checkout failed."), QMessageBox::Ok );
		return;
	}
	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionClone_triggered()
{
	stopUI();
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

//------------------------------------------------------------------------------
void MainWindow::on_actionUndo_triggered()
{
	// Gather Undo actions
	QStringList res;

	if(!runFossil(QStringList() << "undo" << "--explain", &res ))
		return;

	if(res.length()>0 && res[0]=="No undo or redo is available")
		return;

	if(!FileActionDialog::run(this, tr("Undo"), tr("The following actions will be undone. Are you sure?"), res))
		return;

	// Do Undo
	runFossil(QStringList() << "undo" );

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionAbout_triggered()
{
	QMessageBox::about(this, "About... Fuel ",
					   QCoreApplication::applicationName() + " "+ QCoreApplication::applicationVersion() + " " +
					   tr("a GUI frontend to Fossil SCM\n"
						   "by Kostas Karanikolas\n"
						   "Released under the GNU GPL\n\n"
						   "Icon-set by Deleket - Jojo Mendoza\n"
						   "Available under the CC Attribution-Noncommercial-No Derivate 3.0 License"));
}

//------------------------------------------------------------------------------
void MainWindow::on_actionUpdate_triggered()
{
	QStringList res;

	if(!runFossil(QStringList() << "update" << "--nochange", &res ))
		return;

	if(res.length()==0)
		return;

	if(!FileActionDialog::run(this, tr("Update"), tr("The following files will be update. Are you sure?"), res))
		return;

	// Do Update
	runFossil(QStringList() << "update" );

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionSettings_triggered()
{
	SettingsDialog::run(this, settings);
}
