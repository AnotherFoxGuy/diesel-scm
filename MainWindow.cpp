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

#ifdef QT_WS_WIN
	QString EOL_MARK("\r\n");
#else
	QString EOL_MARK("\n");
#endif

//-----------------------------------------------------------------------------
enum
{
	COLUMN_STATUS,
	COLUMN_PATH,
	COLUMN_FILENAME,
	COLUMN_EXTENSION,
	COLUMN_MODIFIED
};

//-----------------------------------------------------------------------------
static QString QuotePath(const QString &path)
{
	return path;
}

//-----------------------------------------------------------------------------
static QStringList QuotePaths(const QStringList &paths)
{
	QStringList res;
	for(int i=0; i<paths.size(); ++i)
		res.append(QuotePath(paths[i]));
	return res;
}

//-----------------------------------------------------------------------------
typedef QMap<QString, QString> QStringMap;
static QStringMap MakeKeyValue(QStringList lines)
{
	QStringMap res;

	foreach(QString l, lines)
	{
		l = l.trimmed();
		int index = l.indexOf(' ');

		QString key;
		QString value;
		if(index!=-1)
		{
			key = l.left(index).trimmed();
			value = l.mid(index).trimmed();
		}
		else
			key = l;

		res.insert(key, value);
	}
	return res;
}

///////////////////////////////////////////////////////////////////////////////
enum DialogAnswer
{
	ANSWER_YES,
	ANSWER_NO,
	ANSWER_YESALL
};

static DialogAnswer DialogQuery(QWidget *parent, const QString &title, const QString &query, bool yesToAllButton=false)
{
	QMessageBox::StandardButtons buttons =  QMessageBox::Yes|QMessageBox::No;
	if(yesToAllButton)
		buttons |= QMessageBox::YesToAll;

	QMessageBox mb(QMessageBox::Question, title, query, buttons, parent, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::Sheet );
	mb.setDefaultButton(QMessageBox::No);
	mb.setWindowModality(Qt::WindowModal);
	mb.setModal(true);
	mb.exec();
	int res = mb.standardButton(mb.clickedButton());
	if(res==QDialogButtonBox::Yes)
		return ANSWER_YES;
	else if(res==QDialogButtonBox::YesToAll)
		return ANSWER_YESALL;
	return ANSWER_NO;
}

//-----------------------------------------------------------------------------
static bool DialogQueryText(QWidget *parent, const QString &title, const QString &query, QString &text, bool isPassword=false)
{
	QInputDialog dlg(parent, Qt::Sheet);
	dlg.setWindowTitle(title);
	dlg.setInputMode(QInputDialog::TextInput);
	dlg.setWindowModality(Qt::WindowModal);
	dlg.setModal(true);
	dlg.setLabelText(query);
	dlg.setTextValue(text);
	if(isPassword)
		dlg.setTextEchoMode(QLineEdit::Password);

	if(dlg.exec() == QDialog::Rejected)
		return false;

	text = dlg.textValue();
	return true;
}

///////////////////////////////////////////////////////////////////////////////
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

// Native applications on OSX don't use menu icons
#ifdef Q_WS_MACX
	foreach(QAction *a, ui->menuBar->actions())
		a->setIconVisibleInMenu(false);
	foreach(QAction *a, ui->menuFile->actions())
		a->setIconVisibleInMenu(false);
#endif

	loadSettings();
	refresh();
	rebuildRecent();
	fossilAbort = false;
}

//------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
	stopUI();
	saveSettings();
	delete ui;
}
//-----------------------------------------------------------------------------
const QString &MainWindow::getCurrentWorkspace()
{
	return currentWorkspace;
}

//-----------------------------------------------------------------------------
void MainWindow::setCurrentWorkspace(const QString &workspace)
{
	if(workspace.isEmpty())
	{
		currentWorkspace.clear();
		return;
	}

	QString new_workspace = QDir(workspace).absolutePath();

	currentWorkspace = new_workspace;
	addWorkspace(new_workspace);

	if(!QDir::setCurrent(new_workspace))
		QMessageBox::critical(this, tr("Error"), tr("Could not change current diectory to ")+new_workspace, QMessageBox::Ok );
}

//------------------------------------------------------------------------------
void MainWindow::addWorkspace(const QString &dir)
{
	if(dir.isEmpty())
		return;

	QDir d(dir);
	QString new_workspace = d.absolutePath();

	// Do not add the workspace if it exists already
	if(workspaceHistory.indexOf(new_workspace)!=-1)
		return;

	workspaceHistory.append(new_workspace);
	rebuildRecent();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionRefresh_triggered()
{
	refresh();
}

//------------------------------------------------------------------------------
bool MainWindow::openWorkspace(const QString &dir)
{
	setCurrentWorkspace(dir);

	on_actionClearLog_triggered();
	stopUI();

	// If this repository is not valid, remove it from the history
	if(!refresh())
	{
		setCurrentWorkspace("");
		workspaceHistory.removeAll(dir);
		rebuildRecent();
		return false;
	}
	return true;
}

//------------------------------------------------------------------------------
void MainWindow::on_actionOpen_triggered()
{
	QString path = QFileDialog::getExistingDirectory(this, tr("Fossil Checkout"), QDir::currentPath());
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
	if(!action)
		return;

	QString workspace = action->data().toString();

	openWorkspace(workspace);
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

	// Retrieve the status of files tracked by fossil
	QStringList res;
	if(!runFossil(QStringList() << "ls" << "-l", &res, SILENT_STATUS))
		return;

	bool scan_files = ui->actionViewUnknown->isChecked();

	workspaceFiles.clear();
	if(scan_files)
	{
		setStatus("Scanning Workspace...");
		QCoreApplication::processEvents();

		RecurseDirectory(all_files, wkdir, wkdir);

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
		setStatus("");
	}


	for(QStringList::iterator it=res.begin(); it!=res.end(); ++it)
	{
		QString line = (*it).trimmed();
		if(line.length()==0)
			continue;

		QString status_text = line.left(10).trimmed();
		QString fname = line.right(line.length() - 10).trimmed();
		RepoFile::EntryType type = RepoFile::TYPE_UNKNOWN;

		// Generate a RepoFile for all non-existant fossil files
		// or for all files if we skipped scanning the workspace
		bool add_missing = !scan_files;

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

		// Filter unwanted file types
		if( ((type & RepoFile::TYPE_MODIFIED) && !ui->actionViewModified->isChecked()) ||
			((type & RepoFile::TYPE_UNCHANGED) && !ui->actionViewUnchanged->isChecked() ))
		{
			workspaceFiles.remove(fname);
			continue;
		}

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
		const char *status_icon= ":icons/icons/Button Blank Gray-01.png"; // Default icon

		for(size_t t=0; t<COUNTOF(stats); ++t)
		{
			if(e.getType() == stats[t].type)
			{
				tag = stats[t].tag;
				tooltip = stats[t].tooltip;
				status_icon = stats[t].icon;
				break;
			}
		}

		QStandardItem *status = new QStandardItem(QIcon(status_icon), tag);
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

    // We need to determine the reason why fossil has failed
	// so we delay processing of the exit_code
	if(!runFossilRaw(QStringList() << "info", &res, &exit_code, SILENT_STATUS))
		return REPO_NOT_FOUND;

	bool run_ok = exit_code == EXIT_SUCCESS;

	for(QStringList::iterator it=res.begin(); it!=res.end(); ++it)
	{
		int col_index = it->indexOf(':');
		if(col_index==-1)
			continue;

		QString key = it->left(col_index).trimmed();
		QString value = it->mid(col_index+1).trimmed();

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
static QString ParseFossilQuery(QString line)
{
	// Extract question
	int qend = line.indexOf('(');
	if(qend == -1)
		qend = line.indexOf('[');
	Q_ASSERT(qend!=-1);
	line = line.left(qend);
	line = line.trimmed();
	line += "?";
	line[0]=QString(line[0]).toUpper()[0];
	return line;
}

//------------------------------------------------------------------------------
// Run fossil. Returns true if execution was succesfull regardless if fossil
// issued an error
bool MainWindow::runFossilRaw(const QStringList &args, QStringList *output, int *exitCode, bool silent, bool detached)
{
	if(!silent)
		log("> fossil "+args.join(" ")+"\n");

	QString wkdir = getCurrentWorkspace();

	QString fossil = getFossilPath();

	if(detached)
	{
		return QProcess::startDetached(fossil, args, wkdir);
	}

	QProcess process(this);
	process.setProcessChannelMode(QProcess::MergedChannels);
	process.setWorkingDirectory(wkdir);

	process.start(fossil, args);
	if(!process.waitForStarted())
	{
		log("Could not start fossil executable '" + fossil + "''\n");
		return false;
	}

	QString ans_yes = 'y' + EOL_MARK;
	QString ans_no = 'n' + EOL_MARK;
	QString ans_always = 'a' + EOL_MARK;

	fossilAbort = false;
	QString buffer;

	while(process.state()==QProcess::Running)
	{
		if(fossilAbort)
		{
			log("\n* Terminated *\n");
			#ifdef Q_WS_WIN
				fossilUI.kill(); // QT on windows cannot terminate console processes with QProcess::terminate
			#else
				process.terminate();
			#endif
			break;
		}

		process.waitForReadyRead(500);
		buffer += process.readAll();

		QCoreApplication::processEvents();

		if(buffer.isEmpty())
			continue;

		// Extract the last line
		int last_line_start = buffer.lastIndexOf(EOL_MARK);

		QString last_line;
		if(last_line_start != -1)
			last_line = buffer.mid(last_line_start+EOL_MARK.length());
		else
			last_line = buffer;

		last_line = last_line.trimmed();

		// Check if we have a query
		bool ends_qmark = !last_line.isEmpty() && last_line[last_line.length()-1]=='?';
		bool have_yn_query = last_line.toLower().indexOf("y/n")!=-1;
		int have_yna_query = last_line.toLower().indexOf("a=always/y/n")!=-1 || last_line.toLower().indexOf("yes/no/all")!=-1;
		bool have_query = ends_qmark && (have_yn_query || have_yna_query);

		// Flush only the unneccessary part of the buffer to the log
		QStringList log_lines = buffer.left(last_line_start).split(EOL_MARK);
		foreach(QString line, log_lines)
		{
			line = line.trimmed();
			if(line.isEmpty())
				continue;

			if(output)
				output->append(line);

			if(!silent)
				log(line+"\n");
		}

		// Remove everything we processed
		buffer = buffer.mid(last_line_start);

		// Now process any query
		if(have_query && have_yna_query)
		{
			QString query = ParseFossilQuery(last_line);
			DialogAnswer res = DialogQuery(this, "Fossil", query, true);
			if(res==ANSWER_YES)
			{
				process.write(ans_yes.toAscii());
				log("Y\n");
			}
			else if(res==ANSWER_YESALL)
			{
				process.write(ans_always.toAscii());
				log("A\n");
			}
			else
			{
				process.write(ans_no.toAscii());
				log("N\n");
			}
			buffer.clear();
		}
		else if(have_query && have_yn_query)
		{
			QString query = ParseFossilQuery(last_line);
			DialogAnswer res = DialogQuery(this, "Fossil", query, false);

			if(res==ANSWER_YES)
			{
				process.write(ans_yes.toAscii());
				log("Y\n");
			}
			else
			{
				process.write(ans_no.toAscii());
				log("N\n");
			}

			buffer.clear();
		}
	}

	// Must be finished by now
	Q_ASSERT(process.state()==QProcess::NotRunning);

	QProcess::ExitStatus es = process.exitStatus();

	if(es!=QProcess::NormalExit)
		return false;

	if(exitCode)
		*exitCode = process.exitCode();

	return true;
}


//------------------------------------------------------------------------------
QString MainWindow::getFossilPath()
{
	// Use the user-specified fossil if available
	if(!settings.fossilPath.isEmpty())
		return QDir::toNativeSeparators(settings.fossilPath);

	QString fossil_exe = "fossil";
#ifdef Q_WS_WIN32
	fossil_exe += ".exe";
#endif
	// Use our fossil if available
	QString fuel_fossil = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + QDir::separator() + fossil_exe);

	if(QFile::exists(fuel_fossil))
		return fuel_fossil;

	// Otherwise assume there is a "fossil" executable in the path
	return fossil_exe;
}
//------------------------------------------------------------------------------
void MainWindow::loadSettings()
{
	// Linux: ~/.config/organizationName/applicationName.conf
	// Windows: HKEY_CURRENT_USER\Software\organizationName\Fuel
	QSettings qsettings(QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());

	if(qsettings.contains("FossilPath"))
		settings.fossilPath = qsettings.value("FossilPath").toString();

	int num_wks = qsettings.beginReadArray("Workspaces");
	for(int i=0; i<num_wks; ++i)
	{
		qsettings.setArrayIndex(i);
		QString wk = qsettings.value("Path").toString();

		// Skip invalid workspaces
		if(wk.isEmpty() || !QDir(wk).exists())
			continue;

		addWorkspace(wk);

		if(qsettings.contains("Active") && qsettings.value("Active").toBool())
			setCurrentWorkspace(wk);
	}
	qsettings.endArray();


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

	if(qsettings.contains("ViewUnknown"))
		ui->actionViewUnknown->setChecked(qsettings.value("ViewUnknown").toBool());
	if(qsettings.contains("ViewModified"))
		ui->actionViewModified->setChecked(qsettings.value("ViewModified").toBool());
	if(qsettings.contains("ViewUnchanged"))
		ui->actionViewUnchanged->setChecked(qsettings.value("ViewUnchanged").toBool());

}

//------------------------------------------------------------------------------
void MainWindow::saveSettings()
{
	QSettings qsettings(QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());

	// If we have a customize fossil path, save it
	if(!settings.fossilPath.isEmpty())
		qsettings.setValue("FossilPath", settings.fossilPath);

	qsettings.beginWriteArray("Workspaces", workspaceHistory.size());
	for(int i=0; i<workspaceHistory.size(); ++i)
	{
		qsettings.setArrayIndex(i);
		qsettings.setValue("Path", workspaceHistory[i]);
		if(getCurrentWorkspace() == workspaceHistory[i])
			qsettings.setValue("Active", true);
        else
            qsettings.remove("Active");
	}
	qsettings.endArray();

	qsettings.setValue("WindowX", x());
	qsettings.setValue("WindowY", y());
	qsettings.setValue("WindowWidth", width());
	qsettings.setValue("WindowHeight", height());
	qsettings.setValue("ViewUnknown", ui->actionViewUnknown->isChecked());
	qsettings.setValue("ViewModified", ui->actionViewModified->isChecked());
	qsettings.setValue("ViewUnchanged", ui->actionViewUnchanged->isChecked());
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

	fossilUI.setParent(this);
	fossilUI.setProcessChannelMode(QProcess::MergedChannels);
	fossilUI.setWorkingDirectory(getCurrentWorkspace());

	log("> fossil ui\n");
	QString fossil = getFossilPath();

	fossilUI.start(fossil, QStringList() << "ui");
	if(!fossilUI.waitForStarted() || fossilUI.state()!=QProcess::Running)
	{
		log(fossil+ tr(" does not exist") +"\n");
		ui->actionFossilUI->setChecked(false);
		return false;
	}

#if 0
	QString buffer;
	while(buffer.indexOf(EOL_MARK)==-1)
	{
		fossilUI.waitForReadyRead(500);
		buffer += fossilUI.readAll();
		QCoreApplication::processEvents();
	}

	fossilUIPort.clear();

	// Parse output to determine the running port
	// "Listening for HTTP requests on TCP port 8080"
	int idx = buffer.indexOf("TCP Port ");
	if(idx!=-1)
		fossilUIPort = buffer.mid(idx, 4);
	else
		fossilUIPort = "8080"; // Have a sensible default if we failed to parse the message
#else
	fossilUIPort = "8080";
#endif


	ui->actionFossilUI->setChecked(true);

	return true;
}

//------------------------------------------------------------------------------
void MainWindow::stopUI()
{
	if(uiRunning())
	{
#ifdef Q_WS_WIN
		fossilUI.kill(); // QT on windows cannot terminate console processes with QProcess::terminate
#else
		fossilUI.terminate();
#endif
	}
	ui->actionFossilUI->setChecked(false);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionFossilUI_triggered()
{
	if(!uiRunning())
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

	QDesktopServices::openUrl(QUrl(getFossilHttpAddress()+"/timeline"));
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
				QDesktopServices::openUrl(QUrl(getFossilHttpAddress()+"/finfo?name="+*it));
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
	getSelectionFilenames(modified_files, RepoFile::TYPE_MODIFIED, true);

	if(modified_files.empty())
		return;

	QString msg;
	if(!CommitDialog::run(this, msg, commitMessages, modified_files))
		return;

	// Since via the commit dialog the user can deselect all files
	if(modified_files.empty())
		return;

	// Do commit
	commitMessages.push_front(msg);
	QString comment_fname;
	{
		QTemporaryFile temp_file;
		if(!temp_file.open())
		{
			QMessageBox::critical(this, tr("Error"), tr("Could not generate comment file"), QMessageBox::Ok );
			return;
		}
		comment_fname = temp_file.fileName();
	}

	QFile comment_file(comment_fname);
	if(!comment_file.open(QIODevice::WriteOnly))
	{
		QMessageBox::critical(this, tr("Error"), tr("Could not generate comment file"), QMessageBox::Ok );
		return;
	}

	comment_file.write(msg.toUtf8());
	comment_file.close();

	runFossil(QStringList() << "commit" << "--message-file" << QuotePath(comment_fname) << QuotePaths(modified_files) );
	QFile::remove(comment_fname);

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
	QString new_name = QInputDialog::getText(this, tr("Rename"), tr("Enter new name"), QLineEdit::Normal, fi_before.filePath(), &ok, Qt::Sheet );
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
				QDir::currentPath(),
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

	on_actionClearLog_triggered();

	QFileInfo path_info(path);
	Q_ASSERT(path_info.dir().exists());
	QString wkdir = path_info.absoluteDir().absolutePath();

	setCurrentWorkspace(wkdir);
	if(!QDir::setCurrent(wkdir))
	{
		QMessageBox::critical(this, tr("Error"), tr("Could not change current diectory"), QMessageBox::Ok );
		return;
	}

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
	// FIXME: Implement this
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
	QString fossil_ver;
	QStringList res;

	if(runFossil(QStringList() << "version", &res, true) && res.length()==1)
	{
		int off = res[0].indexOf("version ");
		if(off!=-1)
			fossil_ver = tr("Fossil version ")+res[0].mid(off) + "\n\n";
	}

	QMessageBox::about(this, "About Fuel...",
					   QCoreApplication::applicationName() + " "+ QCoreApplication::applicationVersion() + " " +
						tr("a GUI frontend to the Fossil SCM\n"
							"by Kostas Karanikolas\n"
							"Released under the GNU GPL\n\n")
					   + fossil_ver +
						tr("Icon-set by Deleket - Jojo Mendoza\n"
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
	// Also retrieve the fossil global settings
	QStringList out;
	if(!runFossil(QStringList() << "settings", &out, true))
		return;

	QStringMap kv = MakeKeyValue(out);
	struct { const char *command; QString *value; } maps[] =
	{
		{ "gdiff-command", &settings.gDiffCommand },
		{ "gmerge-command", &settings.gMergeCommand }
	};

	for(size_t m=0; m<COUNTOF(maps); ++m)
	{
		if(!kv.contains(maps[m].command))
			continue;

		QString value = kv[maps[m].command];
		if(value.indexOf("(global)") != -1)
		{
			int i = value.indexOf(" ");
			Q_ASSERT(i!=-1);
			*maps[m].value =  value.mid(i).trimmed();
		}
	}

	// Run the dialog
	if(SettingsDialog::run(this, settings))
	{
		// Apply settings
		for(size_t m=0; m<COUNTOF(maps); ++m)
		{
			if(maps[m].value->isEmpty())
				runFossil(QStringList() << "unset" << maps[m].command << "-global");
			else
				runFossil(QStringList() << "settings" << maps[m].command << *maps[m].value << "-global");
		}
	}

}

//------------------------------------------------------------------------------
void MainWindow::on_actionSyncSettings_triggered()
{
	QString current;
	// Retrieve existing url
	QStringList out;
	if(runFossil(QStringList() << "remote-url", &out, true) && out.length()==1)
		current = out[0].trimmed();

	if(!DialogQueryText(this, tr("Remote URL"), tr("Enter new remote URL:"), current))
		return;

	QUrl url(current);
	if(!url.isValid())
	{
		QMessageBox::critical(this, tr("Remote URL"), tr("This URL is not valid"), QMessageBox::Ok );
		return;
	}

	// Run as silent to avoid displaying credentials in the log
	runFossil(QStringList() << "remote-url" << QuotePath(url.toString()), 0, true);
}


//------------------------------------------------------------------------------
void MainWindow::on_actionViewModified_triggered()
{
	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionViewUnchanged_triggered()
{
	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionViewUnknown_triggered()
{
	refresh();
}

//------------------------------------------------------------------------------
QString MainWindow::getFossilHttpAddress()
{
	return "http://127.0.0.1:"+fossilUIPort;
}

