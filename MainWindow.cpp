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
#include <QDrag>
#include <QMimeData>
#include <QFileIconProvider>
#include "CommitDialog.h"
#include "FileActionDialog.h"
#include <QDebug>
#include "Utils.h"

#define COUNTOF(array) (sizeof(array)/sizeof(array[0]))

#ifdef QT_WS_WIN
	const QString EOL_MARK("\r\n");
#else
	const QString EOL_MARK("\n");
#endif

#define PATH_SEP			"/"
#define FOSSIL_CHECKOUT1	"_FOSSIL_"
#define FOSSIL_CHECKOUT2	".fslckout"

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
MainWindow::MainWindow(QWidget *parent, QString *workspacePath) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
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

	viewMode = VIEWMODE_TREE;
	loadSettings();

	// Apply any explict workspace path if available
	if(workspacePath && !workspacePath->isEmpty())
		openWorkspace(*workspacePath);

	refresh();
	rebuildRecent();

	// Select the Root of the tree to update the file view
	selectRootDir();

	fossilAbort = false;
}

//------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
	stopUI();
	saveSettings();

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
			if(QMessageBox::Yes !=DialogQuery(this, tr("Open Fossil"), "A workspace does not exist in this folder.\nWould you like to create one here?"))
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
	QString filter(tr("Fossil Files (*.fossil _FOSSIL_ .fslckout)"));

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
	QString filter(tr("Repositories (*.fossil)"));

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
	if(QMessageBox::Yes != DialogQuery(this, tr("Create Workspace"), "Would you like to create a workspace in the same folder?"))
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

	if(QMessageBox::Yes !=DialogQuery(this, tr("Close Workspace"), "Are you sure want to close this workspace?"))
		return;

	// Close Repo
	if(!runFossil(QStringList() << "close"))
	{
		QMessageBox::critical(this, tr("Error"), tr("Cannot close the workspace.\nAre there still uncommitted changes in available?"), QMessageBox::Ok );
		return;
	}

	stopUI();
	setCurrentWorkspace("");
	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionCloneRepository_triggered()
{
	// FIXME: Implement this
	stopUI();
}

//------------------------------------------------------------------------------
void MainWindow::rebuildRecent()
{
	for(int i = 0; i < MAX_RECENT; ++i)
		recentWorkspaceActs[i]->setVisible(false);

	int enabled_acts = qMin<int>(MAX_RECENT, workspaceHistory.size());

	for(int i = 0; i < enabled_acts; ++i)
	{
		QString text = tr("&%1 %2").arg(i + 1).arg(QDir::toNativeSeparators(workspaceHistory[i]));

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
bool MainWindow::scanDirectory(QFileInfoList &entries, const QString& dirPath, const QString &baseDir, const QString ignoreSpec)
{
	QDir dir(dirPath);

	setStatus(dirPath);
	QCoreApplication::processEvents();

	QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
	for (int i=0; i<list.count(); ++i)
	{
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
			if(!scanDirectory(entries, filepath, baseDir, ignoreSpec))
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
		setStatus(tr("Old fossil schema detected. Consider running rebuild."));
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
	if(!runFossil(QStringList() << "ls" << "-l", &res, RUNGLAGS_SILENT_ALL))
		return;

	bool scan_files = ui->actionViewUnknown->isChecked();

	setStatus(tr("Scanning Workspace..."));
	setEnabled(false);
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Dispose RepoFiles
	for(filemap_t::iterator it = workspaceFiles.begin(); it!=workspaceFiles.end(); ++it)
		delete *it;

	workspaceFiles.clear();
	pathSet.clear();

	if(scan_files)
	{
		QCoreApplication::processEvents();

		QString ignore;
		// If we should not be showing ignored files, fill in the ignored spec
		if(!ui->actionViewIgnored->isChecked())
		{
			// QDir expects multiple specs being separated by a semicolon
			ignore = settings.Mappings[FUEL_SETTING_IGNORE_GLOB].Value.toString().replace(',',';');
		}

		scanDirectory(all_files, wkdir, wkdir, ignore);

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
	if(!runFossil(QStringList() << "stash" << "ls", &res, RUNGLAGS_SILENT_ALL))
		return;

	// 19: [5c46757d4b9765] on 2012-04-22 04:41:15
	QRegExp stash_rx("\\s*(\\d+):\\s+\\[(.*)\\] on (\\d+)-(\\d+)-(\\d+) (\\d+):(\\d+):(\\d+)", Qt::CaseInsensitive);

	for(QStringList::iterator line_it=res.begin(); line_it!=res.end(); )
	{
		QString line = *line_it;

		int index = stash_rx.indexIn(line);
		if(index==-1)
			break;

		QString id = stash_rx.cap(1);
		++line_it;

		QString name;
		// Finish at an anonymous stash or start of a new stash ?
		if(line_it==res.end() || stash_rx.indexIn(*line_it)!=-1)
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
	updateDirView();
	updateFileView();
	updateStashView();

	setEnabled(true);
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

		// Find the child that matches this subdir
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
	// File View
	// Clear all rows (except header)
	repoFileModel.clear();

	QStringList header;
	header << tr("S") << tr("File") << tr("Ext") << tr("Modified");

	bool multiple_dirs = selectedDirs.count()>1;

	if(viewMode==VIEWMODE_LIST || multiple_dirs)
		header << tr("Path");

	repoFileModel.setHorizontalHeaderLabels(header);

	struct { RepoFile::EntryType type; const char *tag; const char *tooltip; const char *icon; }
	stats[] =
	{
		{   RepoFile::TYPE_EDITTED, "E", "Editted", ":icons/icons/Button Blank Yellow-01.png" },
		{   RepoFile::TYPE_UNCHANGED, "U", "Unchanged", ":icons/icons/Button Blank Green-01.png" },
		{   RepoFile::TYPE_ADDED, "A", "Added", ":icons/icons/Button Add-01.png" },
		{   RepoFile::TYPE_DELETED, "D", "Deleted", ":icons/icons/Button Close-01.png" },
		{   RepoFile::TYPE_RENAMED, "R", "Renamed", ":icons/icons/Button Reload-01.png" },
		{   RepoFile::TYPE_MISSING, "M", "Missing", ":icons/icons/Button Help-01.png" },
	};

	QFileIconProvider icon_provider;

	size_t item_id=0;
	for(filemap_t::iterator it = workspaceFiles.begin(); it!=workspaceFiles.end(); ++it)
	{
		const RepoFile &e = *it.value();
		QString path = e.getPath();

		// In Tree mode, filter all items not included in the current dir
		if(viewMode==VIEWMODE_TREE && !selectedDirs.contains(path))
			continue;

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
		repoFileModel.setItem(item_id, COLUMN_STATUS, status);

		QFileInfo finfo = e.getFileInfo();
		QIcon icon = icon_provider.icon(finfo);

		QStandardItem *filename_item = 0;
		if(viewMode==VIEWMODE_LIST || multiple_dirs)
		{
			repoFileModel.setItem(item_id, COLUMN_PATH, new QStandardItem(path));
			filename_item = new QStandardItem(icon, QDir::toNativeSeparators(e.getFilePath()));
		}
		else // In Tree mode the path is implicit so the file name is enough
		filename_item = new QStandardItem(icon, e.getFilename());

		Q_ASSERT(filename_item);
		// Keep the path in the user data
		filename_item->setData(e.getFilePath());
		repoFileModel.setItem(item_id, COLUMN_FILENAME, filename_item);

		repoFileModel.setItem(item_id, COLUMN_EXTENSION, new QStandardItem(finfo .completeSuffix()));
		repoFileModel.setItem(item_id, COLUMN_MODIFIED, new QStandardItem(finfo .lastModified().toString(Qt::SystemLocaleShortDate)));

		++item_id;
	}

	ui->tableView->horizontalHeader()->setResizeMode(COLUMN_STATUS, QHeaderView::ResizeToContents);
	ui->tableView->horizontalHeader()->setResizeMode(COLUMN_FILENAME, QHeaderView::Stretch);
	ui->tableView->horizontalHeader()->setResizeMode(COLUMN_EXTENSION, QHeaderView::ResizeToContents);
	ui->tableView->horizontalHeader()->setResizeMode(COLUMN_MODIFIED, QHeaderView::ResizeToContents);
	ui->tableView->horizontalHeader()->setResizeMode(COLUMN_PATH, QHeaderView::ResizeToContents);
	ui->tableView->horizontalHeader()->setSortIndicatorShown(true);
	ui->tableView->resizeRowsToContents();
}

//------------------------------------------------------------------------------
MainWindow::RepoStatus MainWindow::getRepoStatus()
{
	QStringList res;
	int exit_code = EXIT_FAILURE;

	// We need to determine the reason why fossil has failed
	// so we delay processing of the exit_code
	if(!runFossilRaw(QStringList() << "info", &res, &exit_code, RUNGLAGS_SILENT_ALL))
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
		repoStashModel.appendRow(item);
	}
	ui->tableViewStash->resizeColumnsToContents();
	ui->tableViewStash->resizeRowsToContents();
}

//------------------------------------------------------------------------------
void MainWindow::log(const QString &text, bool isHTML)
{
	if(isHTML)
		ui->textBrowser->insertHtml(text);
	else
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
// Run fossil. Returns true if execution was successful regardless if fossil
// issued an error
bool MainWindow::runFossilRaw(const QStringList &args, QStringList *output, int *exitCode, int runFlags)
{
	bool silent_input = (runFlags & RUNGLAGS_SILENT_INPUT) != 0;
	bool silent_output = (runFlags & RUNGLAGS_SILENT_OUTPUT) != 0;
	bool detached = (runFlags & RUNGLAGS_DETACHED) != 0;

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
	{
		return QProcess::startDetached(fossil, args, wkdir);
	}

	QProcess process(this);
	process.setProcessChannelMode(QProcess::MergedChannels);
	process.setWorkingDirectory(wkdir);

	process.start(fossil, args);
	if(!process.waitForStarted())
	{
		log(tr("Could not start fossil executable '") + fossil + "''\n");
		return false;
	}

	QString ans_yes = 'y' + EOL_MARK;
	QString ans_no = 'n' + EOL_MARK;
	QString ans_always = 'a' + EOL_MARK;

	fossilAbort = false;
	QString buffer;

	while(true)
	{
		QProcess::ProcessState state = process.state();
		qint64 bytes_avail = process.bytesAvailable();

		if(state!=QProcess::Running && bytes_avail<1)
			break;

		if(fossilAbort)
		{
			log("\n* "+tr("Terminated")+" *\n");
			#ifdef Q_WS_WIN
				process.kill(); // QT on windows cannot terminate console processes with QProcess::terminate
			#else
				process.terminate();
			#endif
			break;
		}

		process.waitForReadyRead(500);
		QByteArray input = process.readAll();

		#ifdef QT_DEBUG // Log fossil output in debug builds
		if(!input.isEmpty())
			qDebug() << input;
		#endif

		buffer += input;

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
		int have_an_query = last_line.toLower().indexOf("a=always/n")!=-1;

		bool have_query = ends_qmark && (have_yn_query || have_yna_query || have_an_query);

		// Flush only the unnecessary part of the buffer to the log
		QStringList log_lines = buffer.left(last_line_start).split(EOL_MARK);
		foreach(QString line, log_lines)
		{
			line = line.trimmed();
			if(line.isEmpty())
				continue;

			if(output)
				output->append(line);

			if(!silent_output)
				log(line+"\n");
		}

		// Remove everything we processed
		buffer = buffer.mid(last_line_start);

		// Now process any query
		if(have_query && have_yna_query)
		{
			log(last_line);
			QString query = ParseFossilQuery(last_line);
			QMessageBox::StandardButton res = DialogQuery(this, "Fossil", query, QMessageBox::YesToAll|QMessageBox::Yes|QMessageBox::No);
			if(res==QMessageBox::Yes)
			{
				process.write(ans_yes.toAscii());
				log("Y\n");
			}
			else if(res==QMessageBox::YesAll)
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
			log(last_line);
			QString query = ParseFossilQuery(last_line);
			QMessageBox::StandardButton res = DialogQuery(this, "Fossil", query);

			if(res==QMessageBox::Yes)
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
		else if(have_query && have_an_query)
		{
			log(last_line);
			QString query = ParseFossilQuery(last_line);
			QMessageBox::StandardButton res = DialogQuery(this, "Fossil", query, QMessageBox::YesToAll|QMessageBox::No);
			if(res==QMessageBox::YesAll)
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
	QString fossil_path = settings.Mappings[FUEL_SETTING_FOSSIL_PATH].Value.toString();
	if(!fossil_path.isEmpty())
		return QDir::toNativeSeparators(fossil_path);

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

	if(qsettings.contains(FUEL_SETTING_FOSSIL_PATH))
		settings.Mappings[FUEL_SETTING_FOSSIL_PATH].Value = qsettings.value(FUEL_SETTING_FOSSIL_PATH);

	if(qsettings.contains(FUEL_SETTING_COMMIT_MSG))
		settings.Mappings[FUEL_SETTING_COMMIT_MSG].Value = qsettings.value(FUEL_SETTING_COMMIT_MSG);

	if(qsettings.contains(FUEL_SETTING_FILE_DBLCLICK))
		settings.Mappings[FUEL_SETTING_FILE_DBLCLICK].Value = qsettings.value(FUEL_SETTING_FILE_DBLCLICK);

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
	if(qsettings.contains("ViewIgnored"))
		ui->actionViewIgnored->setChecked(qsettings.value("ViewIgnored").toBool());
	if(qsettings.contains("ViewAsList"))
	{
		ui->actionViewAsList->setChecked(qsettings.value("ViewAsList").toBool());
		viewMode = qsettings.value("ViewAsList").toBool()? VIEWMODE_LIST : VIEWMODE_TREE;
	}
	ui->treeView->setVisible(viewMode == VIEWMODE_TREE);

	if(qsettings.contains("ViewStash"))
		ui->actionViewStash->setChecked(qsettings.value("ViewStash").toBool());
	ui->tableViewStash->setVisible(ui->actionViewStash->isChecked());
}

//------------------------------------------------------------------------------
void MainWindow::saveSettings()
{
	QSettings qsettings(QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());

	// If we have a customize fossil path, save it
	QString fossil_path = settings.Mappings[FUEL_SETTING_FOSSIL_PATH].Value.toString();
	qsettings.setValue(FUEL_SETTING_FOSSIL_PATH, fossil_path);
	qsettings.setValue(FUEL_SETTING_COMMIT_MSG, settings.Mappings[FUEL_SETTING_COMMIT_MSG].Value);
	qsettings.setValue(FUEL_SETTING_FILE_DBLCLICK, settings.Mappings[FUEL_SETTING_FILE_DBLCLICK].Value);

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
	qsettings.setValue("ViewIgnored", ui->actionViewIgnored->isChecked());
	qsettings.setValue("ViewAsList", ui->actionViewAsList->isChecked());
	qsettings.setValue("ViewStash", ui->actionViewStash->isChecked());
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
	return runFossil(QStringList() << "gdiff" << QuotePath(repoFile), 0, RUNGLAGS_DETACHED);
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
		log("Fossil UI is already running\n");
		return true;
	}

	fossilUI.setParent(this);
	fossilUI.setProcessChannelMode(QProcess::MergedChannels);
	fossilUI.setWorkingDirectory(getCurrentWorkspace());

	log("<b>&gt; fossil ui</b><br>", true);
	log("Starting Fossil UI. Please wait.\n");
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
	int action = settings.Mappings[FUEL_SETTING_FILE_DBLCLICK].Value.toInt();
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

    QStringList commit_files = modified_files;
	QStringList commit_msgs = settings.Mappings[FUEL_SETTING_COMMIT_MSG].Value.toStringList();

	QString msg;
	bool aborted = !CommitDialog::run(this, tr("Commit Changes"), commit_files, msg, &commit_msgs);

	// Aborted or not we always keep the commit messages.
	// (This has saved me way too many times on TortoiseSVN)
	if(commit_msgs.indexOf(msg)==-1)
	{
		commit_msgs.push_front(msg);
		settings.Mappings[FUEL_SETTING_COMMIT_MSG].Value = commit_msgs;
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

	comment_file.write(msg.toUtf8());
	comment_file.close();

    // Generate fossil parameters.
    // When all files are selected avoid explicitly specifying filenames.
    // This is necessary when commiting after a merge where fossil thinks that
    // we a doing a partial commit, which is not allowed in this case.
    QStringList params;
    params << "commit" << "--message-file" << QuotePath(comment_fname);

    if(modified_files != commit_files)
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

	if(runFossil(QStringList() << "version", &res, RUNGLAGS_SILENT_ALL) && res.length()==1)
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
void MainWindow::loadFossilSettings()
{
	// Also retrieve the fossil global settings
	QStringList out;
	if(!runFossil(QStringList() << "settings", &out, RUNGLAGS_SILENT_ALL))
		return;

	QStringMap kv = MakeKeyValues(out);

	for(Settings::mappings_t::iterator it=settings.Mappings.begin(); it!=settings.Mappings.end(); ++it)
	{
		const QString &name = it.key();
		Settings::Setting::SettingType type = it->Type;

		// Internal types are handled explicitly
		if(type == Settings::Setting::TYPE_INTERNAL)
			continue;

		// Command types we issue directly on fossil
		if(type == Settings::Setting::TYPE_FOSSIL_COMMAND)
		{
			// Retrieve existing url
			QStringList out;
			if(runFossil(QStringList() << name, &out, RUNGLAGS_SILENT_ALL) && out.length()==1)
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
	for(Settings::mappings_t::iterator it=settings.Mappings.begin(); it!=settings.Mappings.end(); ++it)
	{
		const QString &name = it.key();
		Settings::Setting::SettingType type = it.value().Type;

		// Internal types are handled explicitly
		if(type == Settings::Setting::TYPE_INTERNAL)
			continue;

		// Command types we issue directly on fossil
		if(type == Settings::Setting::TYPE_FOSSIL_COMMAND)
		{
			// Run as silent to avoid displaying credentials in the log
			runFossil(QStringList() << "remote-url" << QuotePath(it.value().Value.toString()), 0, RUNGLAGS_SILENT_INPUT);
			continue;
		}

		Q_ASSERT(type == Settings::Setting::TYPE_FOSSIL_GLOBAL || type == Settings::Setting::TYPE_FOSSIL_LOCAL);

		QString value = it.value().Value.toString();
		QStringList params;

		if(value.isEmpty())
			params << "unset" << name;
		else
			params << "settings" << name << "\"" + value + "\"";

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
	return "http://127.0.0.1:"+fossilUIPort;
}

//------------------------------------------------------------------------------
void MainWindow::onTreeViewSelectionChanged(const QItemSelection &/*selected*/, const QItemSelection &/*deselected*/)
{
	selectedDirs.clear();

	QModelIndexList selection = ui->treeView->selectionModel()->selectedIndexes();
	int num_selected = selection.count();

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
	QString new_name = QInputDialog::getText(this, tr("Rename Folder"), tr("Enter new name"), QLineEdit::Normal, old_name, &ok, Qt::Sheet);
	if(!ok || old_name==new_name)
		return;

	const char* invalid_tokens[] = {
		"/", "\\", "\\\\", ":", ">", "<", "*", "?", "|", "\"", ".."
	};

	for(size_t i=0; i<COUNTOF(invalid_tokens); ++i)
	{
		if(new_name.indexOf(invalid_tokens[i])!=-1)
		{
			QMessageBox::critical(this, tr("Error"), tr("Cannot rename folder.\nFolder name contains invalid characters."));
			return;
		}
	}

	QString new_path = old_path.left(dir_start) + new_name;

	if(pathSet.contains(new_path))
	{
		QMessageBox::critical(this, tr("Error"), tr("Cannot rename folder.\nThis folder exists already."));
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
	if(!FileActionDialog::run(this, tr("Rename Folder"), tr("Renaming folder '")+old_path+tr("' to '")+new_path
							  +tr("'\nThe following files will be moved in the repository. Are you sure?"),
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
			log(tr("Move aborted due to errors\n"));
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

		log(tr("Creating folder '")+target_path+"'\n");
		if(!wkdir.mkpath(new_paths[i] + PATH_SEP + "."))
		{
			QMessageBox::critical(this, tr("Error"), tr("Cannot make target folder '")+target_path+"'\n");
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
			QMessageBox::critical(this, tr("Error"), tr("Target file '")+new_file_path+tr("' exists already"));
			goto _exit;
		}

		log(tr("Copying file '")+r->getFilePath()+tr("' to '")+new_file_path+"'\n");

		if(!QFile::copy(r->getFilePath(), new_file_path))
		{
			QMessageBox::critical(this, tr("Error"), tr("Cannot copy file '")+r->getFilePath()+tr("' to '")+new_file_path+"'");
			goto _exit;
		}
	}

	// Finally delete old files
	for(int i=0; i<files_to_move.length(); ++i)
	{
		RepoFile *r = files_to_move[i];

		log(tr("Removing old file '")+r->getFilePath()+"'\n");

		if(!QFile::exists(r->getFilePath()))
		{
			QMessageBox::critical(this, tr("Error"), tr("Source file '")+r->getFilePath()+tr("' does not exist"));
			goto _exit;
		}

		if(!QFile::remove(r->getFilePath()))
		{
			QMessageBox::critical(this, tr("Error"), tr("Cannot remove file '")+r->getFilePath()+"'");
			goto _exit;
		}
	}

	log(tr("Folder renamed completed. Don't forget to commit!\n"));

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
	if(!FileActionDialog::run(this, tr("Apply Stash"), tr("The following stashes will be applied. Are you sure?"), stashes, tr("Delete after applying"), &delete_stashes))
		return;

	// Apply stashes
	for(QStringList::iterator it=stashes.begin(); it!=stashes.end(); ++it)
	{
		stashmap_t::iterator id_it = stashMap.find(*it);
		Q_ASSERT(id_it!=stashMap.end());

		if(!runFossil(QStringList() << "stash" << "apply" << *id_it))
		{
			log(tr("Stash application aborted due to errors\n"));
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
			log(tr("Stash deletion aborted due to errors\n"));
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

	if(!FileActionDialog::run(this, tr("Delete Stashes"), tr("The following stashes will be deleted. Are you sure?"), stashes))
		return;

	// Delete stashes
	for(QStringList::iterator it=stashes.begin(); it!=stashes.end(); ++it)
	{
		stashmap_t::iterator id_it = stashMap.find(*it);
		Q_ASSERT(id_it!=stashMap.end());

		if(!runFossil(QStringList() << "stash" << "drop" << *id_it))
		{
			log(tr("Stash deletion aborted due to errors\n"));
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
	QString uris;

	// text/uri-list is a new-line separate list of uris
	foreach(QString f, filenames)
	{
		uris += QUrl::fromLocalFile(getCurrentWorkspace()+QDir::separator()+f).toString() + '\n';
	}

	QMimeData *mime_data = new QMimeData;
	mime_data->setData("text/uri-list", uris.toUtf8());

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
