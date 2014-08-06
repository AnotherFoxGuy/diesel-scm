#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDrag>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QProgressBar>
#include <QSettings>
#include <QShortcut>
#include <QStandardItem>
#include <QTemporaryFile>
#include <QTextCodec>
#include <QUrl>
#include "CommitDialog.h"
#include "FileActionDialog.h"
#include "CloneDialog.h"
#include "Utils.h"
#include "LoggedProcess.h"

#define COUNTOF(array)			(sizeof(array)/sizeof(array[0]))

#define PATH_SEP				"/"

static const unsigned char		UTF8_BOM[] = { 0xEF, 0xBB, 0xBF };

// 19: [5c46757d4b9765] on 2012-04-22 04:41:15
static const QRegExp			REGEX_STASH("\\s*(\\d+):\\s+\\[(.*)\\] on (\\d+)-(\\d+)-(\\d+) (\\d+):(\\d+):(\\d+)", Qt::CaseInsensitive);


//-----------------------------------------------------------------------------
enum
{
	COLUMN_STATUS,
	COLUMN_FILENAME,
	COLUMN_EXTENSION,
	COLUMN_MODIFIED,
	COLUMN_PATH
};

enum
{
	TAB_LOG,
	TAB_BROWSER
};

enum
{
	REPODIRMODEL_ROLE_PATH = Qt::UserRole+1
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
static QStringMap MakeKeyValues(QStringList lines)
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
class ScopedStatus
{
public:
	ScopedStatus(const QString &text, Ui::MainWindow *mw, QProgressBar *bar) : ui(mw), progressBar(bar)
	{
		ui->statusBar->showMessage(text);
		progressBar->setHidden(false);
	}

	~ScopedStatus()
	{
		ui->statusBar->clearMessage();
		progressBar->setHidden(true);
	}
private:
	Ui::MainWindow *ui;
	QProgressBar *progressBar;
};

///////////////////////////////////////////////////////////////////////////////
MainWindow::MainWindow(Settings &_settings, QWidget *parent, QString *workspacePath) :
	QMainWindow(parent),
	ui(new Ui::MainWindow),
	settings(_settings)
{
	ui->setupUi(this);

	QAction *separator = new QAction(this);
	separator->setSeparator(true);

	// TableView
	ui->tableView->setModel(&repoFileModel);

	ui->tableView->addAction(ui->actionDiff);
	ui->tableView->addAction(ui->actionHistory);
	ui->tableView->addAction(ui->actionOpenFile);
	ui->tableView->addAction(ui->actionOpenContaining);
	ui->tableView->addAction(separator);
	ui->tableView->addAction(ui->actionAdd);
	ui->tableView->addAction(ui->actionRevert);
	ui->tableView->addAction(ui->actionRename);
	ui->tableView->addAction(ui->actionDelete);
	connect( ui->tableView,
		SIGNAL( dragOutEvent() ),
		SLOT( onFileViewDragOut() ),
		Qt::DirectConnection );

	QStringList header;
	header << tr("Status") << tr("File") << tr("Extension") << tr("Modified") << tr("Path");
	repoFileModel.setHorizontalHeaderLabels(header);
	repoFileModel.horizontalHeaderItem(COLUMN_STATUS)->setTextAlignment(Qt::AlignCenter);

	// Needed on OSX as the preset value from the GUI editor is not always reflected
	ui->tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
	ui->tableView->horizontalHeader()->setMovable(true);
#else
	ui->tableView->horizontalHeader()->setSectionsMovable(true);
#endif
	ui->tableView->horizontalHeader()->setStretchLastSection(true);

	// TreeView
	ui->treeView->setModel(&repoDirModel);
	connect( ui->treeView->selectionModel(),
		SIGNAL( selectionChanged(const QItemSelection &, const QItemSelection &) ),
		SLOT( onTreeViewSelectionChanged(const QItemSelection &, const QItemSelection &) ),
		Qt::DirectConnection );

	ui->treeView->addAction(ui->actionCommit);
	ui->treeView->addAction(ui->actionOpenFolder);
	ui->treeView->addAction(ui->actionAdd);
	ui->treeView->addAction(ui->actionRevert);
	ui->treeView->addAction(ui->actionDelete);
	ui->treeView->addAction(separator);
	ui->treeView->addAction(ui->actionRenameFolder);
	ui->treeView->addAction(ui->actionOpenFolder);

	// StashView
	ui->tableViewStash->setModel(&repoStashModel);
	ui->tableViewStash->addAction(ui->actionApplyStash);
	ui->tableViewStash->addAction(ui->actionDiffStash);
	ui->tableViewStash->addAction(ui->actionDeleteStash);
	ui->tableViewStash->horizontalHeader()->setSortIndicatorShown(false);

	// Recent Workspaces
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

	// TabWidget
	ui->tabWidget->setCurrentIndex(TAB_LOG);

	// Construct ProgressBar
	progressBar = new QProgressBar();
	progressBar->setMinimum(0);
	progressBar->setMaximum(0);
	progressBar->setMaximumSize(170, 16);
	progressBar->setAlignment(Qt::AlignCenter);
	progressBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	ui->statusBar->insertPermanentWidget(0, progressBar);
	progressBar->setVisible(false);

#ifdef Q_OS_MACX
	// Native applications on OSX don't have menu icons
	foreach(QAction *a, ui->menuBar->actions())
		a->setIconVisibleInMenu(false);
	foreach(QAction *a, ui->menuFile->actions())
		a->setIconVisibleInMenu(false);
#endif

	abortShortcut = new QShortcut(QKeySequence("Escape"), this);
	abortShortcut->setContext(Qt::ApplicationShortcut);
	abortShortcut->setEnabled(false);
	connect(abortShortcut, SIGNAL(activated()), this, SLOT(onAbort()));

	viewMode = VIEWMODE_TREE;

	applySettings();

	// Apply any explicit workspace path if available
	if(workspacePath && !workspacePath->isEmpty())
		openWorkspace(*workspacePath);

	abortOperation = false;

	rebuildRecent();
}

//------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
	stopUI();
	updateSettings();

	// Dispose RepoFiles
	for(filemap_t::iterator it = workspaceFiles.begin(); it!=workspaceFiles.end(); ++it)
		delete *it;

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

	QString new_workspace = QFileInfo(workspace).absoluteFilePath();

	currentWorkspace = new_workspace;
	addWorkspace(new_workspace);

	if(!QDir::setCurrent(new_workspace))
		QMessageBox::critical(this, tr("Error"), tr("Could not change current directory to '%0'").arg(new_workspace), QMessageBox::Ok );
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
// Open a fossil file or workspace path. If no checkout is detected offer to
// open the fossil file.
bool MainWindow::openWorkspace(const QString &path)
{
	QFileInfo fi(path);
	QString wkspace = path;

	if(fi.isFile())
	{
		wkspace = fi.absoluteDir().absolutePath();
		QString checkout_file1 = wkspace + PATH_SEP + FOSSIL_CHECKOUT1;
		QString checkout_file2 = wkspace + PATH_SEP + FOSSIL_CHECKOUT2;

		if(!(QFileInfo(checkout_file1).exists() || QFileInfo(checkout_file2).exists()) )
		{
			if(QMessageBox::Yes !=DialogQuery(this, tr("Open Workspace"), tr("A workspace does not exist in this folder.\nWould you like to create one here?")))
			{
				wkspace = QFileDialog::getExistingDirectory(
							this,
							tr("Select Workspace Folder"),
							wkspace);

				if(wkspace.isEmpty() || !QDir(wkspace).exists())
					return false;
			}

			// Ok open the fossil
			setCurrentWorkspace(wkspace);
			if(!QDir::setCurrent(wkspace))
			{
				QMessageBox::critical(this, tr("Error"), tr("Could not change current directory"), QMessageBox::Ok );
				return false;
			}

			repositoryFile = fi.absoluteFilePath();

			if(!runFossil(QStringList() << "open" << QuotePath(repositoryFile)))
			{
				QMessageBox::critical(this, tr("Error"), tr("Could not open repository."), QMessageBox::Ok );
				return false;
			}
		}
		else
		{
			if(!QDir(wkspace).exists())
				return false;
			setCurrentWorkspace(wkspace);
		}
	}
	else
	{
		if(!QDir(wkspace).exists())
			return false;
		setCurrentWorkspace(wkspace);
	}

	on_actionClearLog_triggered();
	stopUI();

	// If this repository is not valid, remove it from the history
	if(!refresh())
	{
		setCurrentWorkspace("");
		workspaceHistory.removeAll(path);
		rebuildRecent();
		return false;
	}

	// Select the Root of the tree to update the file view
	selectRootDir();
	return true;
}

//------------------------------------------------------------------------------
void MainWindow::on_actionOpenRepository_triggered()
{
	QString filter(tr("Fossil Files") + QString(" (*." FOSSIL_EXT  " " FOSSIL_CHECKOUT1 " " FOSSIL_CHECKOUT2 ")" ));

	QString path = QFileDialog::getOpenFileName(
		this,
		tr("Open Fossil Repository"),
		QDir::currentPath(),
		filter,
		&filter);

	if(path.isEmpty())
		return;

	openWorkspace(path);
}
//------------------------------------------------------------------------------
void MainWindow::on_actionNewRepository_triggered()
{
	QString filter(tr("Fossil Repositories") + QString(" (*." FOSSIL_EXT ")"));

	// Get Repository file
	QString repo_path = QFileDialog::getSaveFileName(
		this,
		tr("New Fossil Repository"),
		QDir::currentPath(),
		filter,
		&filter);

	if(repo_path.isEmpty())
		return;

	if(QFile::exists(repo_path))
	{
		QMessageBox::critical(this, tr("Error"), tr("A repository file already exists.\nRepository creation aborted."), QMessageBox::Ok );
		return;
	}

	QFileInfo repo_path_info(repo_path);
	Q_ASSERT(repo_path_info.dir().exists());

	// Get Workspace path
	QString wkdir = repo_path_info.absoluteDir().absolutePath();
	if(QMessageBox::Yes != DialogQuery(this, tr("Create Workspace"), tr("Would you like to create a workspace in the same folder?")))
	{
		wkdir = QFileDialog::getExistingDirectory(
			this,
			tr("Select Workspace Folder"),
			wkdir);

		if(wkdir.isEmpty() || !QDir(wkdir).exists())
			return;
	}

	stopUI();
	on_actionClearLog_triggered();

	repositoryFile = repo_path_info.absoluteFilePath();

	// Create repository
	if(!runFossil(QStringList() << "new" << QuotePath(repositoryFile)))
	{
		QMessageBox::critical(this, tr("Error"), tr("Could not create repository."), QMessageBox::Ok );
		return;
	}

	// Create workspace
	setCurrentWorkspace(wkdir);
	if(!QDir::setCurrent(wkdir))
	{
		QMessageBox::critical(this, tr("Error"), tr("Could not change current directory"), QMessageBox::Ok );
		return;
	}

	// Disable unknown file filter
	if(!ui->actionViewUnknown->isChecked())
		ui->actionViewUnknown->setChecked(true);

	// Open repo
	if(!runFossil(QStringList() << "open" << QuotePath(repositoryFile)))
	{
		QMessageBox::critical(this, tr("Error"), tr("Could not open repository."), QMessageBox::Ok );
		return;
	}
	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionCloseRepository_triggered()
{
	if(getRepoStatus()!=REPO_OK)
		return;

	if(QMessageBox::Yes !=DialogQuery(this, tr("Close Workspace"), tr("Are you sure you want to close this workspace?")))
		return;

	// Close Repo
	if(!runFossil(QStringList() << "close"))
	{
		QMessageBox::critical(this, tr("Error"), tr("Cannot close the workspace.\nAre there still uncommitted changes available?"), QMessageBox::Ok );
		return;
	}

	stopUI();
	setCurrentWorkspace("");
	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionCloneRepository_triggered()
{
	QUrl url, url_proxy;
	QString repository;

	if(!CloneDialog::run(this, url, repository, url_proxy))
		return;

	stopUI();

	// Actual command
	QStringList cmd = QStringList() << "clone";

	// Log Command
	QStringList logcmd = QStringList() << "fossil" << "clone";

	QString source = url.toString();
	QString logsource = url.toString(QUrl::RemovePassword);
	if(url.isLocalFile())
	{
		source = url.toLocalFile();
		logsource = source;
	}
	cmd << source << repository;
	logcmd << logsource << repository;

	if(!url_proxy.isEmpty())
	{
		cmd << "--proxy" << url_proxy.toString();
		logcmd << "--proxy" << url_proxy.toString(QUrl::RemovePassword);
	}

	log("<b>&gt;"+logcmd.join(" ")+"</b><br>", true);

	// Clone Repo
	if(!runFossil(cmd, 0, RUNFLAGS_SILENT_INPUT))
	{
		QMessageBox::critical(this, tr("Error"), tr("Could not clone the repository"), QMessageBox::Ok);
		return;
	}

	openWorkspace(repository);
}

//------------------------------------------------------------------------------
void MainWindow::rebuildRecent()
{
	for(int i = 0; i < MAX_RECENT; ++i)
		recentWorkspaceActs[i]->setVisible(false);

	int enabled_acts = qMin<int>(MAX_RECENT, workspaceHistory.size());

	for(int i = 0; i < enabled_acts; ++i)
	{
		QString text = QString("&%1 %2").arg(i + 1).arg(QDir::toNativeSeparators(workspaceHistory[i]));

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
bool MainWindow::scanDirectory(QFileInfoList &entries, const QString& dirPath, const QString &baseDir, const QString ignoreSpec, const bool &abort)
{
	QDir dir(dirPath);

	setStatus(dirPath);
	QCoreApplication::processEvents();

	QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
	for (int i=0; i<list.count(); ++i)
	{
		if(abort)
			return false;

		QFileInfo info = list[i];
		QString filename = info.fileName();
		QString filepath = info.filePath();
		QString rel_path = filepath;
		rel_path.remove(baseDir+PATH_SEP);

		// Skip ignored files
		if(!ignoreSpec.isEmpty() && QDir::match(ignoreSpec, rel_path))
			continue;

		if (info.isDir())
		{
			if(!scanDirectory(entries, filepath, baseDir, ignoreSpec, abort))
				return false;
		}
		else
			entries.push_back(info);
	}
	return true;
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
	ui->actionOpenFolder->setEnabled(on);
	ui->actionRenameFolder->setEnabled(on);
	ui->actionNewStash->setEnabled(on);
	ui->actionDeleteStash->setEnabled(on);
	ui->actionDiffStash->setEnabled(on);
	ui->actionApplyStash->setEnabled(on);
}
//------------------------------------------------------------------------------
bool MainWindow::refresh()
{
	// Load repository info
	RepoStatus st = getRepoStatus();

	if(st==REPO_NOT_FOUND)
	{
		setStatus(tr("No workspace detected."));
		enableActions(false);
		repoFileModel.removeRows(0, repoFileModel.rowCount());
		repoDirModel.clear();
		return false;
	}
	else if(st==REPO_OLD_SCHEMA)
	{
		setStatus(tr("Old repository schema detected. Consider running 'fossil rebuild'"));
		enableActions(false);
		repoFileModel.removeRows(0, repoFileModel.rowCount());
		repoDirModel.clear();
		return true;
	}

	loadFossilSettings();
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

	if(wkdir.isEmpty())
		return;

	// Retrieve the status of files tracked by fossil
	QStringList res;
	if(!runFossil(QStringList() << "ls" << "-l", &res, RUNFLAGS_SILENT_ALL))
		return;

	bool scan_files = ui->actionViewUnknown->isChecked();

	setStatus(tr("Scanning Workspace..."));
	setBusy(true);
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Dispose RepoFiles
	for(filemap_t::iterator it = workspaceFiles.begin(); it!=workspaceFiles.end(); ++it)
		delete *it;

	workspaceFiles.clear();
	pathSet.clear();

	abortOperation = false;

	if(scan_files)
	{
		QCoreApplication::processEvents();

		QString ignore;
		// If we should not be showing ignored files, fill in the ignored spec
		if(!ui->actionViewIgnored->isChecked())
		{
			// QDir expects multiple specs being separated by a semicolon
			ignore = settings.GetFossilValue(FOSSIL_SETTING_IGNORE_GLOB).toString().replace(',',';');
		}

		if(!scanDirectory(all_files, wkdir, wkdir, ignore, abortOperation))
			goto _done;

		for(QFileInfoList::iterator it=all_files.begin(); it!=all_files.end(); ++it)
		{
			QString filename = it->fileName();
			QString fullpath = it->absoluteFilePath();

			// Skip fossil files
			if(filename == FOSSIL_CHECKOUT1 || filename == FOSSIL_CHECKOUT2 || (!repositoryFile.isEmpty() && QFileInfo(fullpath) == QFileInfo(repositoryFile)))
				continue;

			RepoFile *rf = new RepoFile(*it, RepoFile::TYPE_UNKNOWN, wkdir);
			workspaceFiles.insert(rf->getFilePath(), rf);
			pathSet.insert(rf->getPath());
		}
	}

	setStatus(tr("Updating..."));
	QCoreApplication::processEvents();

	// Update Files and Directories

	for(QStringList::iterator line_it=res.begin(); line_it!=res.end(); ++line_it)
	{
		QString line = (*line_it).trimmed();
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
		else if(status_text=="CONFLICT")
			type = RepoFile::TYPE_CONFLICTED;

		// Filter unwanted file types
		if( ((type & RepoFile::TYPE_MODIFIED) && !ui->actionViewModified->isChecked()) ||
			((type & RepoFile::TYPE_UNCHANGED) && !ui->actionViewUnchanged->isChecked() ))
		{
			workspaceFiles.remove(fname);
			continue;
		}
		else
			add_missing = true;

		filemap_t::iterator it = workspaceFiles.find(fname);

		RepoFile *rf = 0;
		if(add_missing && it==workspaceFiles.end())
		{
			QFileInfo info(wkdir+QDir::separator()+fname);
			rf = new RepoFile(info, type, wkdir);
			workspaceFiles.insert(rf->getFilePath(), rf);
		}

		if(!rf)
		{
			it = workspaceFiles.find(fname);
			Q_ASSERT(it!=workspaceFiles.end());
			rf = *it;
		}

		rf->setType(type);

		QString path = rf->getPath();
		pathSet.insert(path);
	}

	// Load the stash
	stashMap.clear();
	res.clear();
	if(!runFossil(QStringList() << "stash" << "ls", &res, RUNFLAGS_SILENT_ALL))
		goto _done;

	for(QStringList::iterator line_it=res.begin(); line_it!=res.end(); )
	{
		QString line = *line_it;

		int index = REGEX_STASH.indexIn(line);
		if(index==-1)
			break;

		QString id = REGEX_STASH.cap(1);
		++line_it;

		QString name;
		// Finish at an anonymous stash or start of a new stash ?
		if(line_it==res.end() || REGEX_STASH.indexIn(*line_it)!=-1)
			name = line.trimmed();
		else // Named stash
		{
			// Parse stash name
			name = (*line_it);
			name = name.trimmed();
			++line_it;
		}

		stashMap.insert(name, id);
	}


	// Update the file item model
_done:
	updateDirView();
	updateFileView();
	updateStashView();

	setBusy(false);
	setStatus("");
	QApplication::restoreOverrideCursor();
}

//------------------------------------------------------------------------------
static void addPathToTree(QStandardItem &root, const QString &path)
{
	QStringList dirs = path.split('/');
	QStandardItem *parent = &root;

	QString fullpath;
	for(QStringList::iterator it = dirs.begin(); it!=dirs.end(); ++it)
	{
		const QString &dir = *it;
		fullpath += dir;

		// Find the child that matches this subdirectory
		bool found = false;
		for(int r=0; r<parent->rowCount(); ++r)
		{
			QStandardItem *child = parent->child(r, 0);
			Q_ASSERT(child);
			if(child->text() == dir)
			{
				parent = child;
				found = true;
			}
		}

		if(!found) // Generate it
		{
			QStandardItem *child = new QStandardItem(QIcon(":icons/icons/Folder-01.png"), dir);
			child->setData(fullpath); // keep the full path to simplify selection
			parent->appendRow(child);
			parent = child;
		}
		fullpath += '/';
	}
}

//------------------------------------------------------------------------------
void MainWindow::updateDirView()
{
	// Directory View
	repoDirModel.clear();

	QStringList header;
	header << tr("Folders");
	repoDirModel.setHorizontalHeaderLabels(header);

	QStandardItem *root = new QStandardItem(QIcon(":icons/icons/My Documents-01.png"), projectName);
	root->setData(""); // Empty Path
	root->setEditable(false);

	repoDirModel.appendRow(root);
	for(stringset_t::iterator it = pathSet.begin(); it!=pathSet.end(); ++it)
	{
		const QString &dir = *it;
		if(dir.isEmpty())
			continue;

		addPathToTree(*root, dir);
	}
	ui->treeView->expandToDepth(0);
	ui->treeView->sortByColumn(0, Qt::AscendingOrder);
}

//------------------------------------------------------------------------------
void MainWindow::updateFileView()
{
	// Clear content except headers
	repoFileModel.removeRows(0, repoFileModel.rowCount());

	struct { RepoFile::EntryType type; QString text; const char *icon; }
	stats[] =
	{
		{   RepoFile::TYPE_EDITTED, tr("Edited"), ":icons/icons/Button Blank Yellow-01.png" },
		{   RepoFile::TYPE_UNCHANGED, tr("Unchanged"), ":icons/icons/Button Blank Green-01.png" },
		{   RepoFile::TYPE_ADDED, tr("Added"), ":icons/icons/Button Add-01.png" },
		{   RepoFile::TYPE_DELETED, tr("Deleted"), ":icons/icons/Button Close-01.png" },
		{   RepoFile::TYPE_RENAMED, tr("Renamed"), ":icons/icons/Button Reload-01.png" },
		{   RepoFile::TYPE_MISSING, tr("Missing"), ":icons/icons/Button Help-01.png" },
		{   RepoFile::TYPE_CONFLICTED, tr("Conflicted"), ":icons/icons/Button Blank Red-01.png" },
	};

	QFileIconProvider icon_provider;

	bool display_path = viewMode==VIEWMODE_LIST || selectedDirs.count() > 1;

	size_t item_id=0;
	for(filemap_t::iterator it = workspaceFiles.begin(); it!=workspaceFiles.end(); ++it)
	{
		const RepoFile &e = *it.value();
		QString path = e.getPath();

		// In Tree mode, filter all items not included in the current dir
		if(viewMode==VIEWMODE_TREE && !selectedDirs.contains(path))
			continue;

		// Status Column
		QString status_text = QString(tr("Unknown"));
		const char *status_icon_path= ":icons/icons/Button Blank Gray-01.png"; // Default icon

		for(size_t t=0; t<COUNTOF(stats); ++t)
		{
			if(e.getType() == stats[t].type)
			{
				status_text = stats[t].text;
				status_icon_path = stats[t].icon;
				break;
			}
		}

		QStandardItem *status = new QStandardItem(QIcon(status_icon_path), status_text);
		status->setToolTip(status_text);
		repoFileModel.setItem(item_id, COLUMN_STATUS, status);

		QFileInfo finfo = e.getFileInfo();
		QIcon icon = icon_provider.icon(finfo);

		QStandardItem *filename_item = 0;
		repoFileModel.setItem(item_id, COLUMN_PATH, new QStandardItem(path));

		if(display_path)
			filename_item = new QStandardItem(icon, QDir::toNativeSeparators(e.getFilePath()));
		else
			filename_item = new QStandardItem(icon, e.getFilename());

		Q_ASSERT(filename_item);
		// Keep the path in the user data
		filename_item->setData(e.getFilePath());
		repoFileModel.setItem(item_id, COLUMN_FILENAME, filename_item);

		repoFileModel.setItem(item_id, COLUMN_EXTENSION, new QStandardItem(finfo.suffix()));
		repoFileModel.setItem(item_id, COLUMN_MODIFIED, new QStandardItem(finfo.lastModified().toString(Qt::SystemLocaleShortDate)));

		++item_id;
	}

	ui->tableView->resizeRowsToContents();
}

//------------------------------------------------------------------------------
MainWindow::RepoStatus MainWindow::getRepoStatus()
{
	QStringList res;
	int exit_code = EXIT_FAILURE;

	// We need to determine the reason why fossil has failed
	// so we delay processing of the exit_code
	if(!runFossilRaw(QStringList() << "info", &res, &exit_code, RUNFLAGS_SILENT_ALL))
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
void MainWindow::updateStashView()
{
	repoStashModel.clear();

	QStringList header;
	header << tr("Stashes");
	repoStashModel.setHorizontalHeaderLabels(header);

	for(stashmap_t::iterator it=stashMap.begin(); it!=stashMap.end(); ++it)
	{
		QStandardItem *item = new QStandardItem(it.key());
		item->setToolTip(it.key());
		repoStashModel.appendRow(item);
	}
	ui->tableViewStash->resizeColumnsToContents();
	ui->tableViewStash->resizeRowsToContents();
}

//------------------------------------------------------------------------------
void MainWindow::log(const QString &text, bool isHTML)
{
	QTextCursor c = ui->textBrowser->textCursor();
	c.movePosition(QTextCursor::End);
	ui->textBrowser->setTextCursor(c);

	if(isHTML)
		ui->textBrowser->insertHtml(text);
	else
		ui->textBrowser->insertPlainText(text);
}

//------------------------------------------------------------------------------
void MainWindow::setStatus(const QString &text)
{
	ui->statusBar->showMessage(text);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionClearLog_triggered()
{
	ui->textBrowser->clear();
}

//------------------------------------------------------------------------------
bool MainWindow::runFossil(const QStringList &args, QStringList *output, int runFlags)
{
	int exit_code = EXIT_FAILURE;
	if(!runFossilRaw(args, output, &exit_code, runFlags))
		return false;

	return exit_code == EXIT_SUCCESS;
}

//------------------------------------------------------------------------------
static QString ParseFossilQuery(QString line)
{
	// Extract question
	int qend = line.lastIndexOf('(');
	if(qend == -1)
		qend = line.lastIndexOf('[');
	Q_ASSERT(qend!=-1);
	line = line.left(qend);
	line = line.trimmed();
	line += "?";
	line[0]=QString(line[0]).toUpper()[0];
	return line;
}

//------------------------------------------------------------------------------
// Run fossil. Returns true if execution was successful regardless if fossil
// issued an error
bool MainWindow::runFossilRaw(const QStringList &args, QStringList *output, int *exitCode, int runFlags)
{
	bool silent_input = (runFlags & RUNFLAGS_SILENT_INPUT) != 0;
	bool silent_output = (runFlags & RUNFLAGS_SILENT_OUTPUT) != 0;
	bool detached = (runFlags & RUNFLAGS_DETACHED) != 0;

	if(!silent_input)
	{
		QString params;
		foreach(QString p, args)
		{
			if(p.indexOf(' ')!=-1)
				params += '"' + p + "\" ";
			else
				params += p + ' ';
		}
		log("<b>&gt; fossil "+params+"</b><br>", true);
	}

	QString wkdir = getCurrentWorkspace();

	QString fossil = getFossilPath();

	if(detached)
		return QProcess::startDetached(fossil, args, wkdir);

	// Make StatusBar message
	QString status_msg = tr("Running Fossil");
	if(args.length() > 0)
		status_msg = QString("Fossil %0").arg(args[0].toCaseFolded());
	ScopedStatus status(status_msg, ui, progressBar);

	// Create fossil process
	LoggedProcess process(this);
	process.setWorkingDirectory(wkdir);

	process.start(fossil, args);
	if(!process.waitForStarted())
	{
		log(tr("Could not start Fossil executable '%0'").arg(fossil)+"\n");
		return false;
	}
	const QChar EOL_MARK('\n');
	QString ans_yes = 'y' + EOL_MARK;
	QString ans_no = 'n' + EOL_MARK;
	QString ans_always = 'a' + EOL_MARK;
	QString ans_convert = 'c' + EOL_MARK;

	abortOperation = false;
	QString buffer;

#ifdef Q_OS_WIN
	QTextCodec *codec = QTextCodec::codecForName("UTF-8");
#else
	QTextCodec *codec = QTextCodec::codecForLocale();
#endif

	Q_ASSERT(codec);
	QTextDecoder *decoder = codec->makeDecoder();
	Q_ASSERT(decoder);

	while(true)
	{
		QProcess::ProcessState state = process.state();
		qint64 bytes_avail = process.logBytesAvailable();

		if(state!=QProcess::Running && bytes_avail<1)
			break;

		if(abortOperation)
		{
			#ifdef Q_OS_WIN		// Verify this is still true on Qt5
				process.kill(); // QT on windows cannot terminate console processes with QProcess::terminate
			#else
				process.terminate();
			#endif
			break;
		}

		QByteArray input;
		process.getLogAndClear(input);

		#ifdef QT_DEBUG // Log fossil output in debug builds
		if(!input.isEmpty())
			qDebug() << input;
		#endif

		buffer += decoder->toUnicode(input);

		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		if(buffer.isEmpty())
			continue;

		// Normalize line endings
		buffer = buffer.replace("\r\n", "\n");
		buffer = buffer.replace("\r", "\n");

		// Extract the last line
		int last_line_start = buffer.lastIndexOf(EOL_MARK);

		QString last_line;
		QString before_last_line;
		if(last_line_start != -1)
		{
			last_line = buffer.mid(last_line_start+1); // Including the EOL

			// Detect previous line
			if(last_line_start>0)
			{
				int before_last_line_start = buffer.lastIndexOf(EOL_MARK, last_line_start-1);
				// No line before ?
				if(before_last_line_start==-1)
					before_last_line_start = 0; // Use entire line

				// Extract previous line
				before_last_line = buffer.mid(before_last_line_start, last_line_start-before_last_line_start);
			}
		}
		else
			last_line = buffer;

		last_line = last_line.trimmed();

		// Check if we have a query
		bool ends_qmark = !last_line.isEmpty() && last_line[last_line.length()-1]=='?';
		bool have_yn_query = last_line.toLower().indexOf("y/n")!=-1;
		bool have_yna_query = last_line.toLower().indexOf("a=always/y/n")!=-1 || last_line.toLower().indexOf("yes/no/all")!=-1 || last_line.toLower().indexOf("a=all/y/n")!=-1;
		bool have_an_query = last_line.toLower().indexOf("a=always/n")!=-1;
		bool have_acyn_query = last_line.toLower().indexOf("a=all/c=convert/y/n")!=-1;

		bool have_query = ends_qmark && (have_yn_query || have_yna_query || have_an_query || have_acyn_query);

		// Flush all complete lines to the log and output
		QStringList log_lines = buffer.left(last_line_start).split(EOL_MARK);
		for(int l=0; l<log_lines.length(); ++l)
		{
			// Do not output the last line if it not complete
			if(l==log_lines.length()-1 && buffer[buffer.length()-1] != EOL_MARK )
				continue;

			QString line = log_lines[l].trimmed();

			if(line.isEmpty())
				continue;

			if(output)
				output->append(line);

			if(!silent_output)
				log(line+"\n");
		}

		// Remove everything we processed (including the EOL)
		buffer = buffer.mid(last_line_start+1) ;

		// Now process any query
		if(have_query && (have_yna_query || have_acyn_query))
		{
			log(last_line);
			QString query = ParseFossilQuery(last_line);
			QMessageBox::StandardButtons buttons = QMessageBox::YesToAll|QMessageBox::Yes|QMessageBox::No;

			// Add any extra text available to the query
			before_last_line = before_last_line.trimmed();
			if(!before_last_line.isEmpty())
				query = before_last_line + "\n" + query;

			// Map the Convert option to the Apply button
			if(have_acyn_query)
				buttons |= QMessageBox::Apply;

			QMessageBox::StandardButton res = DialogQuery(this, "Fossil", query, buttons);
			if(res==QMessageBox::Yes)
			{
				process.write(ans_yes.toLatin1());
				log("Y\n");
			}
			else if(res==QMessageBox::YesAll)
			{
				process.write(ans_always.toLatin1());
				log("A\n");
			}
			else if(res==QMessageBox::Apply)
			{
				process.write(ans_convert.toLatin1());
				log("C\n");
			}
			else
			{
				process.write(ans_no.toLatin1());
				log("N\n");
			}
			buffer.clear();
		}
		else if(have_query && have_yn_query)
		{
			log(last_line);
			QString query = ParseFossilQuery(last_line);
			QMessageBox::StandardButton res = DialogQuery(this, "Fossil", query);

			if(res==QMessageBox::Yes)
			{
				process.write(ans_yes.toLatin1());
				log("Y\n");
			}
			else
			{
				process.write(ans_no.toLatin1());
				log("N\n");
			}

			buffer.clear();
		}
		else if(have_query && have_an_query)
		{
			log(last_line);
			QString query = ParseFossilQuery(last_line);
			QMessageBox::StandardButton res = DialogQuery(this, "Fossil", query, QMessageBox::YesToAll|QMessageBox::No);
			if(res==QMessageBox::YesAll)
			{
				process.write(ans_always.toLatin1());
				log("A\n");
			}
			else
			{
				process.write(ans_no.toLatin1());
				log("N\n");
			}
			buffer.clear();
		}
	}

	delete decoder;

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
	QString fossil_path = settings.GetValue(FUEL_SETTING_FOSSIL_PATH).toString();
	if(!fossil_path.isEmpty())
		return QDir::toNativeSeparators(fossil_path);

	QString fossil_exe = "fossil";
#ifdef Q_OS_WIN
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
void MainWindow::applySettings()
{
	QSettings *store = settings.GetStore();

	int num_wks = store->beginReadArray("Workspaces");
	for(int i=0; i<num_wks; ++i)
	{
		store->setArrayIndex(i);
		QString wk = store->value("Path").toString();

		// Skip invalid workspaces
		if(wk.isEmpty() || !QDir(wk).exists())
			continue;

		addWorkspace(wk);

		if(store->contains("Active") && store->value("Active").toBool())
			setCurrentWorkspace(wk);
	}
	store->endArray();

	store->beginReadArray("FileColumns");
	for(int i=0; i<repoFileModel.columnCount(); ++i)
	{
		store->setArrayIndex(i);
		if(store->contains("Width"))
		{
			int width = store->value("Width").toInt();
			ui->tableView->setColumnWidth(i, width);
		}

		if(store->contains("Index"))
		{
			int index = store->value("Index").toInt();
			int cur_index = ui->tableView->horizontalHeader()->visualIndex(i);
			ui->tableView->horizontalHeader()->moveSection(cur_index, index);
		}

	}
	store->endArray();


	if(store->contains("WindowX") && store->contains("WindowY"))
	{
		QPoint _pos;
		_pos.setX(store->value("WindowX").toInt());
		_pos.setY(store->value("WindowY").toInt());
		move(_pos);
	}

	if(store->contains("WindowWidth") && store->contains("WindowHeight"))
	{
		QSize _size;
		_size.setWidth(store->value("WindowWidth").toInt());
		_size.setHeight(store->value("WindowHeight").toInt());
		resize(_size);
	}

	if(store->contains("ViewUnknown"))
		ui->actionViewUnknown->setChecked(store->value("ViewUnknown").toBool());
	if(store->contains("ViewModified"))
		ui->actionViewModified->setChecked(store->value("ViewModified").toBool());
	if(store->contains("ViewUnchanged"))
		ui->actionViewUnchanged->setChecked(store->value("ViewUnchanged").toBool());
	if(store->contains("ViewIgnored"))
		ui->actionViewIgnored->setChecked(store->value("ViewIgnored").toBool());
	if(store->contains("ViewAsList"))
	{
		ui->actionViewAsList->setChecked(store->value("ViewAsList").toBool());
		viewMode = store->value("ViewAsList").toBool()? VIEWMODE_LIST : VIEWMODE_TREE;
	}
	ui->treeView->setVisible(viewMode == VIEWMODE_TREE);

	if(store->contains("ViewStash"))
		ui->actionViewStash->setChecked(store->value("ViewStash").toBool());
	ui->tableViewStash->setVisible(ui->actionViewStash->isChecked());

}

//------------------------------------------------------------------------------
void MainWindow::updateSettings()
{
	QSettings *store = settings.GetStore();

	store->beginWriteArray("Workspaces", workspaceHistory.size());
	for(int i=0; i<workspaceHistory.size(); ++i)
	{
		store->setArrayIndex(i);
		store->setValue("Path", workspaceHistory[i]);
		if(getCurrentWorkspace() == workspaceHistory[i])
			store->setValue("Active", true);
		else
			store->remove("Active");
	}
	store->endArray();

	store->beginWriteArray("FileColumns", repoFileModel.columnCount());
	for(int i=0; i<repoFileModel.columnCount(); ++i)
	{
		store->setArrayIndex(i);
		store->setValue("Width", ui->tableView->columnWidth(i));
		int index = ui->tableView->horizontalHeader()->visualIndex(i);
		store->setValue("Index", index);
	}
	store->endArray();

	store->setValue("WindowX", x());
	store->setValue("WindowY", y());
	store->setValue("WindowWidth", width());
	store->setValue("WindowHeight", height());
	store->setValue("ViewUnknown", ui->actionViewUnknown->isChecked());
	store->setValue("ViewModified", ui->actionViewModified->isChecked());
	store->setValue("ViewUnchanged", ui->actionViewUnchanged->isChecked());
	store->setValue("ViewIgnored", ui->actionViewIgnored->isChecked());
	store->setValue("ViewAsList", ui->actionViewAsList->isChecked());
	store->setValue("ViewStash", ui->actionViewStash->isChecked());
}

//------------------------------------------------------------------------------
void MainWindow::selectRootDir()
{
	if(viewMode==VIEWMODE_TREE)
	{
		QModelIndex root_index = ui->treeView->model()->index(0, 0);
		ui->treeView->selectionModel()->select(root_index, QItemSelectionModel::Select);
	}
}

//------------------------------------------------------------------------------
void MainWindow::fossilBrowse(const QString &fossilUrl)
{
	if(!uiRunning())
		ui->actionFossilUI->activate(QAction::Trigger);

	bool use_internal = settings.GetValue(FUEL_SETTING_WEB_BROWSER).toInt() == 1;

	QUrl url = QUrl(getFossilHttpAddress()+fossilUrl);

	if(use_internal)
	{
		ui->webView->load(url);
		ui->tabWidget->setCurrentIndex(TAB_BROWSER);
	}
	else
		QDesktopServices::openUrl(url);
}
//------------------------------------------------------------------------------
void MainWindow::getSelectionFilenames(QStringList &filenames, int includeMask, bool allIfEmpty)
{
	if(QApplication::focusWidget() == ui->treeView)
		getDirViewSelection(filenames, includeMask, allIfEmpty);
	else
		getFileViewSelection(filenames, includeMask, allIfEmpty);
}

//------------------------------------------------------------------------------
void MainWindow::getSelectionPaths(stringset_t &paths)
{
	// Determine the directories selected
	QModelIndexList selection = ui->treeView->selectionModel()->selectedIndexes();
	for(QModelIndexList::iterator mi_it = selection.begin(); mi_it!=selection.end(); ++mi_it)
	{
		const QModelIndex &mi = *mi_it;
		QVariant data = repoDirModel.data(mi, REPODIRMODEL_ROLE_PATH);
		paths.insert(data.toString());
	}
}
//------------------------------------------------------------------------------
// Select all workspace files that match the includeMask
void MainWindow::getAllFilenames(QStringList &filenames, int includeMask)
{
	for(filemap_t::iterator it=workspaceFiles.begin(); it!=workspaceFiles.end(); ++it)
	{
		const RepoFile &e = *(*it);

		// Skip unwanted file types
		if(!(includeMask & e.getType()))
			continue;

		filenames.append(e.getFilePath());
	}
}
//------------------------------------------------------------------------------
void MainWindow::getDirViewSelection(QStringList &filenames, int includeMask, bool allIfEmpty)
{
	// Determine the directories selected
	stringset_t paths;

	QModelIndexList selection = ui->treeView->selectionModel()->selectedIndexes();
	if(!(selection.empty() && allIfEmpty))
	{
		getSelectionPaths(paths);
	}

	// Select the actual files form the selected directories
	for(filemap_t::iterator it=workspaceFiles.begin(); it!=workspaceFiles.end(); ++it)
	{
		const RepoFile &e = *(*it);

		// Skip unwanted file types
		if(!(includeMask & e.getType()))
			continue;

		bool include = true;

		// If we have a limited set of paths to filter, check them
		if(!paths.empty())
			include = false;

		for(stringset_t::iterator p_it=paths.begin(); p_it!=paths.end(); ++p_it)
		{
			const QString &path = *p_it;
			// An empty path is the root folder, so it includes all files
			// If the file's path starts with this, we include id
			if(path.isEmpty() || e.getPath().indexOf(path)==0)
			{
				include = true;
				break;
			}
		}

		if(!include)
			continue;

		filenames.append(e.getFilePath());
	}

}

//------------------------------------------------------------------------------
void MainWindow::getFileViewSelection(QStringList &filenames, int includeMask, bool allIfEmpty)
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

		QVariant data = repoFileModel.data(mi, Qt::UserRole+1);
		QString filename = data.toString();
		filemap_t::iterator e_it = workspaceFiles.find(filename);
		Q_ASSERT(e_it!=workspaceFiles.end());
		const RepoFile &e = *e_it.value();

		// Skip unwanted files
		if(!(includeMask & e.getType()))
			continue;

		filenames.append(filename);
	}
}
//------------------------------------------------------------------------------
void MainWindow::getStashViewSelection(QStringList &stashNames, bool allIfEmpty)
{
	QModelIndexList selection = ui->tableViewStash->selectionModel()->selectedIndexes();
	if(selection.empty() && allIfEmpty)
	{
		ui->tableViewStash->selectAll();
		selection = ui->tableViewStash->selectionModel()->selectedIndexes();
		ui->tableViewStash->clearSelection();
	}

	for(QModelIndexList::iterator mi_it = selection.begin(); mi_it!=selection.end(); ++mi_it)
	{
		const QModelIndex &mi = *mi_it;

		if(mi.column()!=0)
			continue;
		QString name = repoStashModel.data(mi).toString();
		stashNames.append(name);
	}
}

//------------------------------------------------------------------------------
bool MainWindow::diffFile(QString repoFile)
{
	// Run the diff detached
	return runFossil(QStringList() << "gdiff" << QuotePath(repoFile), 0, RUNFLAGS_DETACHED);
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
	{
		log(tr("Fossil UI is already running")+"\n");
		return true;
	}

	fossilUI.setParent(this);
	fossilUI.setProcessChannelMode(QProcess::MergedChannels);
	fossilUI.setWorkingDirectory(getCurrentWorkspace());

	log("<b>&gt; fossil ui</b><br>", true);
	log(tr("Starting Fossil browser UI. Please wait.")+"\n");
	QString fossil = getFossilPath();

	QString port = settings.GetValue(FUEL_SETTING_HTTP_PORT).toString();

	fossilUI.start(fossil, QStringList() << "server" << "--localauth" << "-P" << port );

	if(!fossilUI.waitForStarted() || fossilUI.state()!=QProcess::Running)
	{
		log(tr("Could not start Fossil executable '%s'").arg(fossil)+"\n");
		ui->actionFossilUI->setChecked(false);
		return false;
	}

	ui->actionFossilUI->setChecked(true);
	return true;
}

//------------------------------------------------------------------------------
void MainWindow::stopUI()
{
	if(uiRunning())
	{
#ifdef Q_OS_WIN
		fossilUI.kill(); // QT on windows cannot terminate console processes with QProcess::terminate
#else
		fossilUI.terminate();
#endif
	}
	fossilUI.close();

	ui->actionFossilUI->setChecked(false);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionFossilUI_triggered()
{
	if(!uiRunning() && ui->actionFossilUI->isChecked())
	{
		startUI();
		fossilBrowse("");
	}
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
	fossilBrowse("/timeline");
}

//------------------------------------------------------------------------------
void MainWindow::on_actionHistory_triggered()
{
	QStringList selection;
	getSelectionFilenames(selection);

	for(QStringList::iterator it = selection.begin(); it!=selection.end(); ++it)
		fossilBrowse("/finfo?name="+*it);
}

//------------------------------------------------------------------------------
void MainWindow::on_tableView_doubleClicked(const QModelIndex &/*index*/)
{
	int action = settings.GetValue(FUEL_SETTING_FILE_DBLCLICK).toInt();
	if(action==FILE_DLBCLICK_ACTION_DIFF)
		on_actionDiff_triggered();
	else if(action==FILE_DLBCLICK_ACTION_OPEN)
		on_actionOpenFile_triggered();
	else if(action==FILE_DLBCLICK_ACTION_OPENCONTAINING)
		on_actionOpenContaining_triggered();
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
	QString remote_url = settings.GetFossilValue(FOSSIL_SETTING_REMOTE_URL).toString();

	if(remote_url.isEmpty() || remote_url == "off")
	{
		QMessageBox::critical(this, tr("Error"), tr("A remote repository has not been specified.\nUse the preferences window to set the remote repostory location"), QMessageBox::Ok );
		return;
	}

	runFossil(QStringList() << "push");
}

//------------------------------------------------------------------------------
void MainWindow::on_actionPull_triggered()
{
	QString remote_url = settings.GetFossilValue(FOSSIL_SETTING_REMOTE_URL).toString();

	if(remote_url.isEmpty() || remote_url == "off")
	{
		QMessageBox::critical(this, tr("Error"), tr("A remote repository has not been specified.\nUse the preferences window to set the remote repostory location"), QMessageBox::Ok );
		return;
	}

	runFossil(QStringList() << "pull");
}

//------------------------------------------------------------------------------
void MainWindow::on_actionCommit_triggered()
{
	QStringList commit_files;
	getSelectionFilenames(commit_files, RepoFile::TYPE_MODIFIED, true);

	if(commit_files.empty())
		return;

	QStringList commit_msgs = settings.GetValue(FUEL_SETTING_COMMIT_MSG).toStringList();

	QString msg;
	bool aborted = !CommitDialog::run(this, tr("Commit Changes"), commit_files, msg, &commit_msgs);

	// Aborted or not we always keep the commit messages.
	// (This has saved me way too many times on TortoiseSVN)
	if(commit_msgs.indexOf(msg)==-1)
	{
		commit_msgs.push_front(msg);
		settings.SetValue(FUEL_SETTING_COMMIT_MSG, commit_msgs);
	}

	if(aborted)
		return;

	// Since via the commit dialog the user can deselect all files
	if(commit_files.empty())
		return;

	// Do commit
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

	// Write BOM
	comment_file.write(reinterpret_cast<const char *>(UTF8_BOM), sizeof(UTF8_BOM));

	// Write Comment
	comment_file.write(msg.toUtf8());
	comment_file.close();

	// Generate fossil parameters.
	QStringList params;
	params << "commit" << "--message-file" << QuotePath(comment_fname);

	// When a subset of files has been selected, explicitely specify each file.
	// Otherwise all files will be implicitly committed by fossil. This is necessary
	// when committing after a merge where fossil thinks that we are trying to do
	// a partial commit which is not permitted.
	QStringList all_modified_files;
	getAllFilenames(all_modified_files, RepoFile::TYPE_MODIFIED);

	if(commit_files.size() != all_modified_files.size())
		params  << QuotePaths(commit_files);

	runFossil(params);
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

	if(!FileActionDialog::run(this, tr("Add files"), tr("The following files will be added.")+"\n"+tr("Are you sure?"), selection))
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

	if(!FileActionDialog::run(this, tr("Remove files"), tr("The following files will be removed from the repository.")+"\n"+tr("Are you sure?"), all_files, tr("Also delete the local files"), &remove_local ))
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
			if(fi.exists())
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

	if(!FileActionDialog::run(this, tr("Revert files"), tr("The following files will be reverted.")+"\n"+tr("Are you sure?"), modified_files))
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
	QString new_name = QInputDialog::getText(this, tr("Rename"), tr("New name"), QLineEdit::Normal, fi_before.filePath(), &ok, Qt::Sheet );
	if(!ok)
		return;

	QFileInfo fi_after(new_name);

	if(fi_after.exists())
	{
		QMessageBox::critical(this, tr("Error"), tr("File '%0' already exists.\nRename aborted.").arg(new_name), QMessageBox::Ok );
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

	if(!FileActionDialog::run(this, tr("Undo"), tr("The following actions will be undone.")+"\n"+tr("Are you sure?"), res))
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

	if(runFossil(QStringList() << "version", &res, RUNFLAGS_SILENT_ALL) && res.length()==1)
	{
		int off = res[0].indexOf("version ");
		if(off!=-1)
			fossil_ver = tr("Fossil version %0").arg(res[0].mid(off+8)) + "\n";
	}

	QString qt_ver = tr("QT version %0").arg(QT_VERSION_STR) + "\n\n";

	QMessageBox::about(this, tr("About Fuel..."),
					   QCoreApplication::applicationName() + " "+ QCoreApplication::applicationVersion() + " " +
						tr("a GUI frontend to the Fossil SCM\n"
							"by Kostas Karanikolas\n"
							"Released under the GNU GPL")+"\n\n" +
					   fossil_ver +
					   qt_ver +
						tr("Icons by Deleket - Jojo Mendoza\n"
							"Available under the CC Attribution Noncommercial No Derivative 3.0 License") + "\n\n" +
						tr("Translations with the help of:") + "\n"
							"stayawake (German de_DE)\n"
							"djnavas (Spanish es_ES)\n"
							"Fringale (French fr_FR)\n"
							"mouse166 (Russian ru_RU)\n"
					   );
}

//------------------------------------------------------------------------------
void MainWindow::on_actionUpdate_triggered()
{
	QStringList res;

	if(!runFossil(QStringList() << "update" << "--nochange", &res, RUNFLAGS_SILENT_ALL))
		return;

	if(res.length()==0)
		return;

	if(!FileActionDialog::run(this, tr("Update"), tr("The following files will be updated.")+"\n"+tr("Are you sure?"), res))
		return;

	// Do Update
	runFossil(QStringList() << "update" );

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::loadFossilSettings()
{
	// Also retrieve the fossil global settings
	QStringList out;
	if(!runFossil(QStringList() << "settings", &out, RUNFLAGS_SILENT_ALL))
		return;

	QStringMap kv = MakeKeyValues(out);

	for(Settings::mappings_t::iterator it=settings.GetMappings().begin(); it!=settings.GetMappings().end(); ++it)
	{
		const QString &name = it.key();
		Settings::Setting::SettingType type = it->Type;

		// Command types we issue directly on fossil
		if(type == Settings::Setting::TYPE_FOSSIL_COMMAND)
		{
			// Retrieve existing url
			QStringList out;
			if(runFossil(QStringList() << name, &out, RUNFLAGS_SILENT_ALL) && out.length()==1)
				it.value().Value = out[0].trimmed();

			continue;
		}

		// Otherwise it must be a fossil setting
		if(!kv.contains(name))
			continue;

		QString value = kv[name];
		if(value.indexOf("(global)") != -1 || value.indexOf("(local)") != -1)
		{
			int i = value.indexOf(" ");
			Q_ASSERT(i!=-1);
			value = value.mid(i).trimmed();

			// Remove quotes if any
			if(value.length()>=2 && value.at(0)=='\"' && value.at(value.length()-1)=='\"')
				value = value.mid(1, value.length()-2);

			it.value().Value = value;
		}
	}
}

//------------------------------------------------------------------------------
void MainWindow::on_actionSettings_triggered()
{
	loadFossilSettings();

	// Run the dialog
	if(!SettingsDialog::run(this, settings))
		return;

	// Apply settings
	for(Settings::mappings_t::iterator it=settings.GetMappings().begin(); it!=settings.GetMappings().end(); ++it)
	{
		const QString &name = it.key();
		Settings::Setting::SettingType type = it.value().Type;

		// Command types we issue directly on fossil
		if(type == Settings::Setting::TYPE_FOSSIL_COMMAND)
		{
			// Run as silent to avoid displaying credentials in the log
			runFossil(QStringList() << "remote-url" << QuotePath(it.value().Value.toString()), 0, RUNFLAGS_SILENT_INPUT);
			continue;
		}

		Q_ASSERT(type == Settings::Setting::TYPE_FOSSIL_GLOBAL || type == Settings::Setting::TYPE_FOSSIL_LOCAL);

		QString value = it.value().Value.toString();
		QStringList params;

		if(value.isEmpty())
			params << "unset" << name;
		else
			params << "settings" << name << value;

		if(type == Settings::Setting::TYPE_FOSSIL_GLOBAL)
			params << "-global";

		runFossil(params);
	}
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
void MainWindow::on_actionViewIgnored_triggered()
{
	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionViewAsList_triggered()
{
	viewMode =  ui->actionViewAsList->isChecked() ? VIEWMODE_LIST : VIEWMODE_TREE;
	ui->treeView->setVisible(viewMode == VIEWMODE_TREE);
	updateFileView();
}

//------------------------------------------------------------------------------
QString MainWindow::getFossilHttpAddress()
{
	QString port = settings.GetValue(FUEL_SETTING_HTTP_PORT).toString();
	return "http://127.0.0.1:"+port;
}

//------------------------------------------------------------------------------
void MainWindow::onTreeViewSelectionChanged(const QItemSelection &/*selected*/, const QItemSelection &/*deselected*/)
{
	QModelIndexList selection = ui->treeView->selectionModel()->selectedIndexes();
	int num_selected = selection.count();

	// Do not modify the selection if nothing is selected
	if(num_selected==0)
		return;

	selectedDirs.clear();

	for(int i=0; i<num_selected; ++i)
	{
		QModelIndex index = selection.at(i);
		QString dir = repoDirModel.data(index, REPODIRMODEL_ROLE_PATH).toString();
		selectedDirs.insert(dir);
	}

	updateFileView();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionOpenFolder_triggered()
{
	const QItemSelection &selection =  ui->treeView->selectionModel()->selection();

	if(selection.indexes().count()!=1)
		return;

	QModelIndex index = selection.indexes().at(0);
	on_treeView_doubleClicked(index);
}

//------------------------------------------------------------------------------
void MainWindow::on_treeView_doubleClicked(const QModelIndex &index)
{
	QString target = repoDirModel.data(index, REPODIRMODEL_ROLE_PATH).toString();
	target = getCurrentWorkspace() + PATH_SEP + target;

	QUrl url = QUrl::fromLocalFile(target);
	QDesktopServices::openUrl(url);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionRenameFolder_triggered()
{
	stringset_t paths;
	getSelectionPaths(paths);

	if(paths.size()!=1)
		return;

	QString old_path = *paths.begin();

	// Root Node?
	if(old_path.isEmpty())
	{
		// Cannot change the project name via command line
		// so unsupported
		return;
	}

	int dir_start = old_path.lastIndexOf(PATH_SEP);
	if(dir_start==-1)
		dir_start = 0;
	else
		++dir_start;

	QString old_name = old_path.mid(dir_start);

	bool ok = false;
	QString new_name = QInputDialog::getText(this, tr("Rename Folder"), tr("New name"), QLineEdit::Normal, old_name, &ok, Qt::Sheet);
	if(!ok || old_name==new_name)
		return;

	const char* invalid_tokens[] = {
		"/", "\\", "\\\\", ":", ">", "<", "*", "?", "|", "\"", ".."
	};

	for(size_t i=0; i<COUNTOF(invalid_tokens); ++i)
	{
		if(new_name.indexOf(invalid_tokens[i])!=-1)
		{
			QMessageBox::critical(this, tr("Error"), tr("Cannot rename folder.")+"\n" +tr("Folder name contains invalid characters."));
			return;
		}
	}

	QString new_path = old_path.left(dir_start) + new_name;

	if(pathSet.contains(new_path))
	{
		QMessageBox::critical(this, tr("Error"), tr("Cannot rename folder.")+"\n" +tr("This folder exists already."));
		return;
	}

	// Collect the files to be moved
	filelist_t files_to_move;
	QStringList new_paths;
	QStringList operations;
	foreach(RepoFile *r, workspaceFiles)
	{
		if(r->getPath().indexOf(old_path)!=0)
			continue;

		files_to_move.append(r);
		QString new_dir = new_path + r->getPath().mid(old_path.length());
		new_paths.append(new_dir);
		QString new_file_path =  new_dir + PATH_SEP + r->getFilename();
		operations.append(r->getFilePath() + " -> " + new_file_path);
	}

	if(files_to_move.empty())
		return;

	bool move_local = false;
	if(!FileActionDialog::run(this, tr("Rename Folder"), tr("Renaming folder '%0' to '%1'\n"
							  "The following files will be moved in the repository.").arg(old_path, new_path)+"\n"+tr("Are you sure?"),
							  operations,
							  tr("Also move the workspace files"), &move_local)) {
		return;
	}

	// Rename files in fossil
	Q_ASSERT(files_to_move.length() == new_paths.length());
	for(int i=0; i<files_to_move.length(); ++i)
	{
		RepoFile *r = files_to_move[i];
		const QString &new_file_path = new_paths[i] + PATH_SEP + r->getFilename();

		if(!runFossil(QStringList() << "mv" <<  QuotePath(r->getFilePath()) << QuotePath(new_file_path)))
		{
			log(tr("Move aborted due to errors")+"\n");
			goto _exit;
		}
	}

	if(!move_local)
		goto _exit;

	// First ensure that the target directories exist, and if not make them
	for(int i=0; i<files_to_move.length(); ++i)
	{
		QString target_path = QDir::cleanPath(getCurrentWorkspace() + PATH_SEP + new_paths[i] + PATH_SEP);
		QDir target(target_path);

		if(target.exists())
			continue;

		QDir wkdir(getCurrentWorkspace());
		Q_ASSERT(wkdir.exists());

		log(tr("Creating folder '%0'").arg(target_path)+"\n");
		if(!wkdir.mkpath(new_paths[i] + PATH_SEP + "."))
		{
			QMessageBox::critical(this, tr("Error"), tr("Cannot make target folder '%0'").arg(target_path));
			goto _exit;
		}
	}

	// Now that target directories exist copy files
	for(int i=0; i<files_to_move.length(); ++i)
	{
		RepoFile *r = files_to_move[i];
		QString new_file_path = new_paths[i] + PATH_SEP + r->getFilename();

		if(QFile::exists(new_file_path))
		{
			QMessageBox::critical(this, tr("Error"), tr("Target file '%0' exists already").arg(new_file_path));
			goto _exit;
		}

		log(tr("Copying file '%0' to '%1'").arg(r->getFilePath(), new_file_path)+"\n");

		if(!QFile::copy(r->getFilePath(), new_file_path))
		{
			QMessageBox::critical(this, tr("Error"), tr("Cannot copy file '%0' to '%1'").arg(r->getFilePath(), new_file_path));
			goto _exit;
		}
	}

	// Finally delete old files
	for(int i=0; i<files_to_move.length(); ++i)
	{
		RepoFile *r = files_to_move[i];

		log(tr("Removing old file '%0'").arg(r->getFilePath())+"\n");

		if(!QFile::exists(r->getFilePath()))
		{
			QMessageBox::critical(this, tr("Error"), tr("Source file '%0' does not exist").arg(r->getFilePath()));
			goto _exit;
		}

		if(!QFile::remove(r->getFilePath()))
		{
			QMessageBox::critical(this, tr("Error"), tr("Cannot remove file '%0'").arg(r->getFilePath()));
			goto _exit;
		}
	}

	log(tr("Folder renamed completed. Don't forget to commit!")+"\n");

_exit:
	refresh();
}

//------------------------------------------------------------------------------
QMenu * MainWindow::createPopupMenu()
{
	return NULL;
}

//------------------------------------------------------------------------------
void MainWindow::on_actionViewStash_triggered()
{
	ui->tableViewStash->setVisible(ui->actionViewStash->isChecked());
}

//------------------------------------------------------------------------------
void MainWindow::on_actionNewStash_triggered()
{
	QStringList stashed_files;
	getSelectionFilenames(stashed_files, RepoFile::TYPE_MODIFIED, true);

	if(stashed_files.empty())
		return;

	QString stash_name;
	bool revert = false;
	QString checkbox_text = tr("Revert stashed files");
	if(!CommitDialog::run(this, tr("Stash Changes"), stashed_files, stash_name, 0, true, &checkbox_text, &revert) || stashed_files.empty())
		return;

	stash_name = stash_name.trimmed();

	if(stash_name.indexOf("\"")!=-1 || stash_name.isEmpty())
	{
		QMessageBox::critical(this, tr("Error"), tr("Invalid stash name"));
		return;
	}

	// Check that this stash does not exist
	for(stashmap_t::iterator it=stashMap.begin(); it!=stashMap.end(); ++it)
	{
		if(stash_name == it.key())
		{
			QMessageBox::critical(this, tr("Error"), tr("This stash already exists"));
			return;
		}
	}

	// Do Stash
	QString command = "snapshot";
	if(revert)
		command = "save";

	runFossil(QStringList() << "stash" << command << "-m" << stash_name << QuotePaths(stashed_files) );
	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionApplyStash_triggered()
{
	QStringList stashes;
	getStashViewSelection(stashes);

	bool delete_stashes = false;
	if(!FileActionDialog::run(this, tr("Apply Stash"), tr("The following stashes will be applied.")+"\n"+tr("Are you sure?"), stashes, tr("Delete after applying"), &delete_stashes))
		return;

	// Apply stashes
	for(QStringList::iterator it=stashes.begin(); it!=stashes.end(); ++it)
	{
		stashmap_t::iterator id_it = stashMap.find(*it);
		Q_ASSERT(id_it!=stashMap.end());

		if(!runFossil(QStringList() << "stash" << "apply" << *id_it))
		{
			log(tr("Stash application aborted due to errors")+"\n");
			return;
		}
	}

	// Delete stashes
	for(QStringList::iterator it=stashes.begin(); delete_stashes && it!=stashes.end(); ++it)
	{
		stashmap_t::iterator id_it = stashMap.find(*it);
		Q_ASSERT(id_it!=stashMap.end());

		if(!runFossil(QStringList() << "stash" << "drop" << *id_it))
		{
			log(tr("Stash deletion aborted due to errors")+"\n");
			return;
		}
	}

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionDeleteStash_triggered()
{
	QStringList stashes;
	getStashViewSelection(stashes);

	if(stashes.empty())
		return;

	if(!FileActionDialog::run(this, tr("Delete Stashes"), tr("The following stashes will be deleted.")+"\n"+tr("Are you sure?"), stashes))
		return;

	// Delete stashes
	for(QStringList::iterator it=stashes.begin(); it!=stashes.end(); ++it)
	{
		stashmap_t::iterator id_it = stashMap.find(*it);
		Q_ASSERT(id_it!=stashMap.end());

		if(!runFossil(QStringList() << "stash" << "drop" << *id_it))
		{
			log(tr("Stash deletion aborted due to errors")+"\n");
			return;
		}
	}

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionDiffStash_triggered()
{
	QStringList stashes;
	getStashViewSelection(stashes);

	if(stashes.length() != 1)
		return;

	stashmap_t::iterator id_it = stashMap.find(*stashes.begin());
	Q_ASSERT(id_it!=stashMap.end());

	// Run diff
	runFossil(QStringList() << "stash" << "diff" << *id_it, 0);
}

//------------------------------------------------------------------------------
void MainWindow::onFileViewDragOut()
{
	QStringList filenames;
	getFileViewSelection(filenames);

	if(filenames.isEmpty())
		return;

	QList<QUrl> urls;
	foreach(QString f, filenames)
		urls.append(QUrl::fromLocalFile(getCurrentWorkspace()+QDir::separator()+f));

	QMimeData *mime_data = new QMimeData;
	mime_data->setUrls(urls);

	QDrag *drag = new QDrag(this);
	drag->setMimeData(mime_data);
	drag->exec(Qt::CopyAction);
}

//------------------------------------------------------------------------------
void MainWindow::on_textBrowser_customContextMenuRequested(const QPoint &pos)
{
	QMenu *menu = ui->textBrowser->createStandardContextMenu();
	menu->addSeparator();
	menu->addAction(ui->actionClearLog);
	menu->popup(ui->textBrowser->mapToGlobal(pos));
}

//------------------------------------------------------------------------------
void MainWindow::on_tableView_customContextMenuRequested(const QPoint &pos)
{
	QPoint gpos = QCursor::pos();
#ifdef Q_OS_WIN
	if(qApp->keyboardModifiers() & Qt::SHIFT)
	{
		ui->tableView->selectionModel()->select(ui->tableView->indexAt(pos), QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
		QStringList fnames;
		getSelectionFilenames(fnames);

		if(fnames.size()==1)
		{
			QString fname = getCurrentWorkspace() + PATH_SEP + fnames[0];
			fname = QDir::toNativeSeparators(fname);
			if(ShowExplorerMenu((HWND)winId(), fname, gpos))
				refresh();
		}
	}
	else
#else
	Q_UNUSED(pos);
#endif
	{
		QMenu *menu = new QMenu(this);
		menu->addActions(ui->tableView->actions());
		menu->popup(gpos);
	}

}

//------------------------------------------------------------------------------
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	// Ignore drops from the same window
	if(event->source() != this)
		event->acceptProposedAction();
}

//------------------------------------------------------------------------------
void MainWindow::dropEvent(QDropEvent *event)
{
	QList<QUrl> urls = event->mimeData()->urls();

	if(urls.length()==0)
		return;

	// When dropping a folder or a checkout file, open the associated worksspace
	QFileInfo finfo(urls.first().toLocalFile());
	if(finfo.isDir() || finfo.suffix() == FOSSIL_EXT || finfo.fileName() == FOSSIL_CHECKOUT1 || finfo.fileName() == FOSSIL_CHECKOUT2 )
	{
		event->acceptProposedAction();
		openWorkspace(finfo.absoluteFilePath());
	}
	else // Otherwise if not a workspace file and within a workspace, add
	{
		QStringList newfiles;

		Q_FOREACH(const QUrl &url, urls)
		{
			QFileInfo finfo(url.toLocalFile());
			QString abspath = finfo.absoluteFilePath();

			// Within the current workspace ?
			if(abspath.indexOf(currentWorkspace)!=0)
				continue; // skip

			// Remove workspace from full path
			QString wkpath = abspath.right(abspath.length()-currentWorkspace.length()-1);

			newfiles.append(wkpath);
		}

		// Any files to add?
		if(!newfiles.empty())
		{
			if(!FileActionDialog::run(this, tr("Add files"), tr("The following files will be added.")+"\n"+tr("Are you sure?"), newfiles))
				return;

			// Do Add
			runFossil(QStringList() << "add" << QuotePaths(newfiles) );

			refresh();
		}
	}
}

//------------------------------------------------------------------------------
void MainWindow::setBusy(bool busy)
{
	abortShortcut->setEnabled(busy);
	bool enabled = !busy;
	ui->menuBar->setEnabled(enabled);
	ui->mainToolBar->setEnabled(enabled);
	ui->centralWidget->setEnabled(enabled);
}

//------------------------------------------------------------------------------
void MainWindow::onAbort()
{
	abortOperation = true;
	// FIXME: Rename this to something better, Operation Aborted
	log("<br><b>* "+tr("Terminated")+" *</b><br>", true);
}

//------------------------------------------------------------------------------
void MainWindow::fullRefresh()
{
	refresh();
	// Select the Root of the tree to update the file view
	selectRootDir();
}
