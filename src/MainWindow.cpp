#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDrag>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QInputDialog>
#include <QMimeData>
#include <QProgressBar>
#include <QToolButton>
#include <QLabel>
#include <QSettings>
#include <QShortcut>
#include "SettingsDialog.h"
#include "FslSettingsDialog.h"
#include "SearchBox.h"
#include "CommitDialog.h"
#include "FileActionDialog.h"
#include "CloneDialog.h"
#include "RevisionDialog.h"
#include "RemoteDialog.h"
#include "AboutDialog.h"
#include "UpdateCheckDialog.h"
#include "Utils.h"

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
	ROLE_WORKSPACE_ITEM = Qt::UserRole+1
};

struct WorkspaceItem
{
	enum
	{
		TYPE_UNKNOWN,
		TYPE_WORKSPACE,
		TYPE_FOLDER,
		TYPE_STASHES,
		TYPE_STASH,
		TYPE_BRANCHES,
		TYPE_BRANCH,
		TYPE_TAGS,
		TYPE_TAG,
		TYPE_REMOTES,
		TYPE_REMOTE,
	};

	enum
	{
		STATE_DEFAULT,
		STATE_UNCHANGED,
		STATE_MODIFIED,
		STATE_UNKNOWN
	};


	WorkspaceItem()
	: Type(TYPE_UNKNOWN)
	, State(STATE_DEFAULT)
	{
	}

	WorkspaceItem(int type, const QString &value, int state=STATE_DEFAULT)
	: Type(type), State(state), Value(value)
	{
	}

	WorkspaceItem(const WorkspaceItem &other)
	{
		Type = other.Type;
		State = other.State;
		Value = other.Value;
	}

	int Type;
	int State;
	QString Value;


	operator QVariant() const
	{
		return QVariant::fromValue(*this);
	}
};
Q_DECLARE_METATYPE(WorkspaceItem)

///////////////////////////////////////////////////////////////////////////////
MainWindow::MainWindow(Settings &_settings, QWidget *parent, QString *workspacePath) :
	QMainWindow(parent),
	ui(new Ui::MainWindow),
	settings(_settings)
{
	ui->setupUi(this);

	QAction *separator = new QAction(this);
	separator->setSeparator(true);

	fileActionSeparator = new QAction(this);
	fileActionSeparator->setSeparator(true);

	workspaceActionSeparator = new QAction(this);
	workspaceActionSeparator->setSeparator(true);

	// fileTableView
	ui->fileTableView->setModel(&getWorkspace().getFileModel());

	ui->fileTableView->addAction(ui->actionDiff);
	ui->fileTableView->addAction(ui->actionHistory);
	ui->fileTableView->addAction(ui->actionOpenFile);
	ui->fileTableView->addAction(ui->actionOpenContaining);
	ui->fileTableView->addAction(separator);
	ui->fileTableView->addAction(ui->actionAdd);
	ui->fileTableView->addAction(ui->actionRevert);
	ui->fileTableView->addAction(ui->actionRename);
	ui->fileTableView->addAction(ui->actionDelete);

	connect( ui->fileTableView,
		SIGNAL( dragOutEvent() ),
		SLOT( onFileViewDragOut() ),
		Qt::DirectConnection );

	QStringList header;
	header << tr("Status") << tr("File") << tr("Extension") << tr("Modified") << tr("Path");
	getWorkspace().getFileModel().setHorizontalHeaderLabels(header);
	getWorkspace().getFileModel().horizontalHeaderItem(COLUMN_STATUS)->setTextAlignment(Qt::AlignCenter);

	// Needed on OSX as the preset value from the GUI editor is not always reflected
	ui->fileTableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
	ui->fileTableView->horizontalHeader()->setMovable(true);
#else
	ui->fileTableView->horizontalHeader()->setSectionsMovable(true);
#endif
	ui->fileTableView->horizontalHeader()->setStretchLastSection(true);

	// workspaceTreeView
	ui->workspaceTreeView->setModel(&getWorkspace().getTreeModel());

	header.clear();
	header << tr("Workspace");
	getWorkspace().getTreeModel().setHorizontalHeaderLabels(header);

	connect( ui->workspaceTreeView->selectionModel(),
		SIGNAL( selectionChanged(const QItemSelection &, const QItemSelection &) ),
		SLOT( onWorkspaceTreeViewSelectionChanged(const QItemSelection &, const QItemSelection &) ),
		Qt::DirectConnection );

	// Workspace Menus
	menuWorkspace = new QMenu(this);
	menuWorkspace->addAction(ui->actionCommit);
	menuWorkspace->addAction(ui->actionOpenFolder);
	menuWorkspace->addAction(ui->actionAdd);
	menuWorkspace->addAction(ui->actionRevert);
	menuWorkspace->addAction(ui->actionDelete);
	menuWorkspace->addAction(separator);
	menuWorkspace->addAction(ui->actionRenameFolder);
	menuWorkspace->addAction(ui->actionOpenFolder);

	// StashMenu
	menuStashes = new QMenu(this);
	menuStashes->addAction(ui->actionCreateStash);
	menuStashes->addAction(separator);
	menuStashes->addAction(ui->actionApplyStash);
	menuStashes->addAction(ui->actionDiffStash);
	menuStashes->addAction(ui->actionDeleteStash);

	// TagsMenu
	menuTags = new QMenu(this);
	menuTags->addAction(ui->actionCreateTag);
	menuTags->addAction(separator);
	menuTags->addAction(ui->actionDeleteTag);
	menuTags->addAction(ui->actionUpdate);

	// BranchesMenu
	menuBranches = new QMenu(this);
	menuBranches->addAction(ui->actionCreateBranch);
	menuBranches->addAction(separator);
	menuBranches->addAction(ui->actionMergeBranch);
	menuBranches->addAction(ui->actionUpdate);

	// RemotesMenu
	menuRemotes = new QMenu(this);
	menuRemotes->addAction(ui->actionPushRemote);
	menuRemotes->addAction(ui->actionPullRemote);
	menuRemotes->addAction(separator);
	menuRemotes->addAction(ui->actionAddRemote);
	menuRemotes->addAction(ui->actionDeleteRemote);
	menuRemotes->addAction(ui->actionEditRemote);
	menuRemotes->addAction(ui->actionSetDefaultRemote);

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
	for(int i = 0; i < MAX_RECENT; ++i)
	{
		recentWorkspaceActs[i] = new QAction(this);
		recentWorkspaceActs[i]->setVisible(false);
		connect(recentWorkspaceActs[i], SIGNAL(triggered()), this, SLOT(onOpenRecent()));
		ui->menuFile->insertAction(recent_sep, recentWorkspaceActs[i]);
	}

	// Custom Actions
	for(int i = 0; i < settings.GetCustomActions().size(); ++i)
	{
		customActions[i] = new QAction(this);
		customActions[i]->setVisible(false);
		connect(customActions[i], SIGNAL(triggered()), this, SLOT(onCustomActionTriggered()));
		customActions[i]->setData(i);
		customActions[i]->setShortcut(QKeySequence(QString("Ctrl+%0").arg(i+1)));
	}

	// TabWidget
	ui->tabWidget->setCurrentIndex(TAB_LOG);

	// Tags Label
	lblTags = new QLabel();
	ui->statusBar->insertPermanentWidget(0, lblTags);
	lblTags->setVisible(true);

	// Create Progress Bar
	progressBar = new QProgressBar();
	progressBar->setMinimum(0);
	progressBar->setMaximum(0);
	progressBar->setMaximumSize(170, 16);
	progressBar->setAlignment(Qt::AlignCenter);
	progressBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	ui->statusBar->insertPermanentWidget(1, progressBar);
	progressBar->setVisible(false);

	// Create Abort Button
	abortButton = new QToolButton(ui->statusBar);
	abortButton->setAutoRaise(true);
	abortButton->setIcon(getCachedIcon(":/icons/icon-action-stop"));
	abortButton->setVisible(false);
	abortButton->setArrowType(Qt::NoArrow);
	abortButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	abortButton->setDefaultAction(ui->actionAbortOperation);
	ui->statusBar->insertPermanentWidget(2, abortButton);
	ui->actionAbortOperation->setEnabled(false);

#ifdef Q_OS_MACX
	// Native applications on OSX don't have menu icons
	foreach(QAction *a, ui->menuBar->actions())
		a->setIconVisibleInMenu(false);
	foreach(QAction *a, ui->menuFile->actions())
		a->setIconVisibleInMenu(false);

	// For some unknown reason on OSX the treeview gets a focus rect. So disable it
	ui->workspaceTreeView->setAttribute(Qt::WA_MacShowFocusRect, false);

	// Tighen-up the sizing of the main widgets to look slightly more consistent with the OSX style
	ui->centralWidget->layout()->setContentsMargins(0, 0, 0, 0);
	ui->workspaceTreeView->setFrameShape(QFrame::NoFrame);
	ui->fileTableView->setFrameShape(QFrame::NoFrame);
	ui->splitterVertical->setHandleWidth(1);
	ui->splitterHorizontal->setHandleWidth(1);

	// Wrong color scheme on Yosemite but better than the standard TabWidget
	ui->tabWidget->setDocumentMode(true);

	// Hide the header since the height is different form the FileView and looks ugly
	//ui->workspaceTreeView->setHeaderHidden(true);
#endif

	// Searchbox
	// Add spacer to pad to right
	QWidget* spacer = new QWidget();
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	ui->mainToolBar->addWidget(spacer);

	// Search shortcut
	searchShortcut = new QShortcut(QKeySequence(QKeySequence::Find), this);
	searchShortcut->setContext(Qt::ApplicationShortcut);
	searchShortcut->setEnabled(true);
	connect(searchShortcut, SIGNAL(activated()), this, SLOT(onSearch()));

	// Create SearchBox
	searchBox = new SearchBox(this);
	searchBox->setPlaceholderText(tr("Filter (%0)").arg(searchShortcut->key().toString(QKeySequence::NativeText)));
	searchBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	searchBox->setMinimumWidth(230);
	ui->mainToolBar->addWidget(searchBox);

	connect( searchBox,
		SIGNAL( textChanged(const QString&)),
		SLOT( onSearchBoxTextChanged(const QString&)),
		Qt::DirectConnection );

	// Add another spacer to the right
	spacer = new QWidget();
	spacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	spacer->setMinimumWidth(3);
	ui->mainToolBar->addWidget(spacer);

	viewMode = VIEWMODE_TREE;

	uiCallback.init(this);

	// Need to be before applySettings which sets the last workspace
	getWorkspace().fossil().Init(&uiCallback, settings.GetValue(FUEL_SETTING_FOSSIL_PATH).toString());

	applySettings();

	// Apply any explicit workspace path if available
	if(workspacePath && !workspacePath->isEmpty())
		openWorkspace(*workspacePath);

	operationAborted = false;

	rebuildRecent();
}

//------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
	stopUI();
	getWorkspace().storeWorkspace(*settings.GetStore());
	updateSettings();

	delete ui;
}

//-----------------------------------------------------------------------------
void MainWindow::setCurrentWorkspace(const QString &workspace)
{
	if(!getWorkspace().switchWorkspace(workspace, *settings.GetStore()))
		QMessageBox::critical(this, tr("Error"), tr("Could not change current directory to '%0'").arg(workspace), QMessageBox::Ok );
	else
		addWorkspaceHistory(getWorkspace().getPath());
}

//------------------------------------------------------------------------------
void MainWindow::addWorkspaceHistory(const QString &dir)
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
		QString checkout_file1 = wkspace + PATH_SEPARATOR + FOSSIL_CHECKOUT1;
		QString checkout_file2 = wkspace + PATH_SEPARATOR + FOSSIL_CHECKOUT2;

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

			// Ok open the repository file
			if(!getWorkspace().fossil().createWorkspace(fi.absoluteFilePath(), wkspace))
			{
				QMessageBox::critical(this, tr("Error"), tr("Could not open repository."), QMessageBox::Ok );
				return false;
			}

		}
		else
		{
			if(!QDir(wkspace).exists())
			{
				QMessageBox::critical(this, tr("Error"), tr("Could not open repository."), QMessageBox::Ok );
				return false;
			}
			setCurrentWorkspace(wkspace);
		}
	}
	else
	{
		if(!QDir(wkspace).exists())
		{
			QMessageBox::critical(this, tr("Error"), tr("Could not open repository."), QMessageBox::Ok );
			return false;
		}
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
	searchBox->clear();
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

	// Create repository
	QString repo_abs_path = repo_path_info.absoluteFilePath();

	if(!getWorkspace().fossil().createRepository(repo_abs_path))
	{
		QMessageBox::critical(this, tr("Error"), tr("Could not create repository."), QMessageBox::Ok );
		return;
	}

	if(!getWorkspace().fossil().createWorkspace(repo_abs_path, wkdir))
	{
		QMessageBox::critical(this, tr("Error"), tr("Could not open repository."), QMessageBox::Ok );
		return;
	}

	// Disable unknown file filter
	if(!ui->actionViewUnknown->isChecked())
		ui->actionViewUnknown->setChecked(true);

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionCloseRepository_triggered()
{
	if(getWorkspace().fossil().getWorkspaceState()!=Fossil::WORKSPACE_STATE_OK)
		return;

	if(QMessageBox::Yes !=DialogQuery(this, tr("Close Workspace"), tr("Are you sure you want to close this workspace?")))
		return;

	// Close Repo
	if(!getWorkspace().fossil().closeWorkspace())
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

	if(!getWorkspace().fossil().cloneRepository(repository, url, url_proxy))
	{
		QMessageBox::critical(this, tr("Error"), tr("Could not clone the repository"), QMessageBox::Ok);
		return;
	}

	if(!openWorkspace(repository))
		return;

	// Store credentials
	if(!url.isLocalFile())
	{
		KeychainDelete(this, url, *settings.GetStore());

		if(!KeychainSet(this, url, *settings.GetStore()))
			QMessageBox::critical(this, tr("Error"), tr("Could not store information to keychain."), QMessageBox::Ok );
	}

	// Create Remote
	url.setPassword("");

	QString name = UrlToStringNoCredentials(url);
	getWorkspace().addRemote(url, name);
	getWorkspace().setRemoteDefault(url);
	updateWorkspaceView();
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
void MainWindow::enableActions(bool on)
{
	QAction *actions[] = {
		ui->actionCloseRepository,
		ui->actionCommit,
		ui->actionDiff,
		ui->actionAdd,
		ui->actionDelete,
		ui->actionPush,
		ui->actionPull,
		ui->actionRename,
		ui->actionHistory,
		ui->actionFossilUI,
		ui->actionRevert,
		ui->actionTimeline,
		ui->actionOpenFile,
		ui->actionOpenContaining,
		ui->actionUndo,
		ui->actionUpdate,
		ui->actionOpenFolder,
		ui->actionRenameFolder,
		ui->actionCreateStash,
		ui->actionDeleteStash,
		ui->actionDiffStash,
		ui->actionApplyStash,
		ui->actionDeleteStash,
		ui->actionCreateTag,
		ui->actionDeleteTag,
		ui->actionCreateBranch,
		ui->actionMergeBranch,
		ui->actionFossilSettings,
		ui->actionViewAll,
		ui->actionViewAsFolders,
		ui->actionViewAsList,
		ui->actionViewIgnored,
		ui->actionViewModifedOnly,
		ui->actionViewModified,
		ui->actionViewUnchanged,
		ui->actionViewUnknown
	};

	for(size_t i=0; i<COUNTOF(actions); ++i)
		actions[i]->setEnabled(on);
}

//------------------------------------------------------------------------------
bool MainWindow::refresh()
{
	QString title = "Fuel";

	loadFossilSettings();

	bool valid = scanWorkspace();
	if(valid)
	{
		const QString &project_name = getWorkspace().getProjectName();
		if(!project_name.isEmpty())
			title += " - " + project_name;
	}

	enableActions(valid);
	setWindowTitle(title);
	return valid;
}

//------------------------------------------------------------------------------
bool MainWindow::scanWorkspace()
{
	setBusy(true);
	bool valid = true;

	// Load repository info
	Fossil::WorkspaceState st = getWorkspace().fossil().getWorkspaceState();
	QString status;

	if(st==Fossil::WORKSPACE_STATE_NOTFOUND)
	{
		status = tr("No workspace detected.");
		valid = false;
	}
	else if(st==Fossil::WORKSPACE_STATE_OLDSCHEMA)
	{
		status = tr("Old repository schema detected. Consider running 'fossil rebuild'");
		valid = false;
	}

	versionList.clear();
	selectedTags.clear();
	selectedBranches.clear();

	if(valid)
	{
		// Determine ignored file patterns
		QStringList ignore_patterns;
		{
			static const QRegExp REGEX_GLOB_LIST(",|\\n", Qt::CaseSensitive);

			QString ignore_list = settings.GetFossilValue(FOSSIL_SETTING_IGNORE_GLOB).toString().trimmed();

			// Read patterns from versionable file if it exists
			QFile ignored_file(getWorkspace().getPath() + PATH_SEPARATOR ".fossil-settings" PATH_SEPARATOR "ignore-glob");

			if(ignored_file.open(QFile::ReadOnly))
			{
				ignore_list = ignored_file.readAll();
				ignored_file.close();
			}

			if(!ignore_list.isEmpty())
				ignore_patterns = ignore_list.split(REGEX_GLOB_LIST, QString::SkipEmptyParts);

			TrimStringList(ignore_patterns);
		}

		getWorkspace().scanWorkspace(ui->actionViewUnknown->isChecked(),
								ui->actionViewIgnored->isChecked(),
								ui->actionViewModified->isChecked(),
								ui->actionViewUnchanged->isChecked(),
								ignore_patterns,
								uiCallback
								);

		// Build default versions list
		versionList += getWorkspace().getBranches();
		versionList += getWorkspace().getTags().keys();
		lblTags->setText(" " + getWorkspace().getActiveTags().join(" ") + " ");
	}

	updateWorkspaceView();
	updateFileView();

	setStatus(status);
	lblTags->setVisible(valid);

	setBusy(false);
	return valid;
}

//------------------------------------------------------------------------------
static void addPathToTree(QStandardItem &root, const QString &path, const QIcon &iconDefault, const QIcon &iconUnchanged, const QIcon &iconModified, const QIcon &iconUnknown, const pathstate_map_t &pathState)
{
	QStringList dirs = path.split(PATH_SEPARATOR);
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
			int state = WorkspaceItem::STATE_DEFAULT;

			pathstate_map_t::const_iterator state_it = pathState.find(fullpath);
			if(state_it != pathState.end())
			{
				WorkspaceFile::Type type = state_it.value();

				if(type & (WorkspaceFile::TYPE_MODIFIED))
					state = WorkspaceItem::STATE_MODIFIED;
				else if(type == WorkspaceFile::TYPE_UNKNOWN)
					state = WorkspaceItem::STATE_UNKNOWN;
				else
					state = WorkspaceItem::STATE_UNCHANGED;
			}

			QStandardItem *child = new QStandardItem(dir);
			child->setData(WorkspaceItem(WorkspaceItem::TYPE_FOLDER, fullpath, state), ROLE_WORKSPACE_ITEM);

			QString tooltip = fullpath;

			if(state == WorkspaceItem::STATE_UNCHANGED)
			{
				child->setIcon(iconUnchanged);
				tooltip += " " + QObject::tr("Unchanged");
			}
			else if(state == WorkspaceItem::STATE_MODIFIED)
			{
				child->setIcon(iconModified);
				tooltip += " " + QObject::tr("Modified");
			}
			else if(state == WorkspaceItem::STATE_UNKNOWN)
			{
				child->setIcon(iconUnknown);
				tooltip += " " + QObject::tr("Unknown");
			}
			else
				child->setIcon(iconDefault);

			child->setToolTip(tooltip);

			parent->appendRow(child);
			parent = child;
		}

		fullpath += PATH_SEPARATOR;
	}
}

//------------------------------------------------------------------------------
void MainWindow::updateWorkspaceView()
{
	// Record expanded tree-node names, and selection
	name_modelindex_map_t name_map;
	BuildNameToModelIndex(name_map, getWorkspace().getTreeModel());
	stringset_t expanded_items;
	stringset_t selected_items;

	const QItemSelection selection = ui->workspaceTreeView->selectionModel()->selection();

	for(name_modelindex_map_t::const_iterator it=name_map.begin(); it!=name_map.end(); ++it)
	{
		const QModelIndex mi = it.value();
		if(ui->workspaceTreeView->isExpanded(mi))
			expanded_items.insert(it.key());

		if(selection.contains(mi))
			selected_items.insert(it.key());
	}

	// Clear content except headers
	getWorkspace().getTreeModel().removeRows(0, getWorkspace().getTreeModel().rowCount());

	QStandardItem *workspace = new QStandardItem(getCachedIcon(":icons/icon-item-folder"), tr("Files") );
	workspace->setData(WorkspaceItem(WorkspaceItem::TYPE_WORKSPACE, ""), ROLE_WORKSPACE_ITEM);
	workspace->setEditable(false);

	getWorkspace().getTreeModel().appendRow(workspace);
	if(viewMode == VIEWMODE_TREE)
	{
		// FIXME: Change paths to map to allow for automatic sorting
		QStringList paths = getWorkspace().getPaths().toList();
		paths.sort();

		foreach(const QString &dir, paths)
		{
			if(dir.isEmpty())
				continue;

			addPathToTree(*workspace, dir,
						  getCachedIcon(":icons/icon-item-folder"),
						  getCachedIcon(":icons/icon-item-folder-unchanged"),
						  getCachedIcon(":icons/icon-item-folder-modified"),
						  getCachedIcon(":icons/icon-item-folder-unknown"),
						  getWorkspace().getPathState());
		}

		// Expand root folder
		ui->workspaceTreeView->setExpanded(workspace->index(), true);
	}

	// Branches
	QStandardItem *branches = new QStandardItem(getCachedIcon(":icons/icon-item-branch"), tr("Branches"));
	branches->setData(WorkspaceItem(WorkspaceItem::TYPE_BRANCHES, ""), ROLE_WORKSPACE_ITEM);
	branches->setEditable(false);
	getWorkspace().getTreeModel().appendRow(branches);
	foreach(const QString &branch_name, getWorkspace().getBranches())
	{
		QStandardItem *branch = new QStandardItem(getCachedIcon(":icons/icon-item-branch"), branch_name);
		branch->setData(WorkspaceItem(WorkspaceItem::TYPE_BRANCH, branch_name), ROLE_WORKSPACE_ITEM);

		bool active = getWorkspace().getActiveTags().contains(branch_name);
		if(active)
		{
			QFont font = branch->font();
			font.setBold(true);
			branch->setFont(font);
		}
		branches->appendRow(branch);
	}

	// Tags
	QStandardItem *tags = new QStandardItem(getCachedIcon(":icons/icon-item-tag"), tr("Tags"));
	tags->setData(WorkspaceItem(WorkspaceItem::TYPE_TAGS, ""), ROLE_WORKSPACE_ITEM);
	tags->setEditable(false);
	getWorkspace().getTreeModel().appendRow(tags);
	for(QStringMap::const_iterator it=getWorkspace().getTags().begin(); it!=getWorkspace().getTags().end(); ++it)
	{
		const QString &tag_name = it.key();

		QStandardItem *tag = new QStandardItem(getCachedIcon(":icons/icon-item-tag"), tag_name);
		tag->setData(WorkspaceItem(WorkspaceItem::TYPE_TAG, tag_name), ROLE_WORKSPACE_ITEM);

		bool active = getWorkspace().getActiveTags().contains(tag_name);
		if(active)
		{
			QFont font = tag->font();
			font.setBold(true);
			tag->setFont(font);
		}
		tags->appendRow(tag);
	}

	// Stashes
	QStandardItem *stashes = new QStandardItem(getCachedIcon(":icons/icon-action-repo-open"), tr("Stashes"));
	stashes->setData(WorkspaceItem(WorkspaceItem::TYPE_STASHES, ""), ROLE_WORKSPACE_ITEM);
	stashes->setEditable(false);
	getWorkspace().getTreeModel().appendRow(stashes);
	for(stashmap_t::const_iterator it= getWorkspace().getStashes().begin(); it!=getWorkspace().getStashes().end(); ++it)
	{
		QStandardItem *stash = new QStandardItem(getCachedIcon(":icons/icon-action-repo-open"), it.key());
		stash->setData(WorkspaceItem(WorkspaceItem::TYPE_STASH, it.value()), ROLE_WORKSPACE_ITEM);
		stashes->appendRow(stash);
	}

	// Remotes
	QStandardItem *remotes = new QStandardItem(getCachedIcon(":icons/icon-item-remote"), tr("Remotes"));
	remotes->setData(WorkspaceItem(WorkspaceItem::TYPE_REMOTES, ""), ROLE_WORKSPACE_ITEM);
	remotes->setEditable(false);
	getWorkspace().getTreeModel().appendRow(remotes);
	for(remote_map_t::const_iterator it=getWorkspace().getRemotes().begin(); it!=getWorkspace().getRemotes().end(); ++it)
	{
		QStandardItem *remote_item = new QStandardItem(getCachedIcon(":icons/icon-item-remote"), it->name);
		remote_item->setData(WorkspaceItem(WorkspaceItem::TYPE_REMOTE, it->url.toString()), ROLE_WORKSPACE_ITEM);

		remote_item->setToolTip(UrlToStringDisplay(it->url));

		// Mark the default url as bold
		if(it->isDefault)
		{
			QFont font = remote_item->font();
			font.setBold(true);
			remote_item->setFont(font);
		}
		remotes->appendRow(remote_item);
	}

	// Expand previously selected nodes
	name_map.clear();
	BuildNameToModelIndex(name_map, getWorkspace().getTreeModel());

	for(stringset_t::const_iterator it=expanded_items.begin(); it!=expanded_items.end(); ++it)
	{
		name_modelindex_map_t::const_iterator mi_it = name_map.find(*it);
		if(mi_it!=name_map.end())
			ui->workspaceTreeView->setExpanded(mi_it.value(), true);
	}

	// Select previous selected item
	for(stringset_t::const_iterator it=selected_items.begin(); it!=selected_items.end(); ++it)
	{
		name_modelindex_map_t::const_iterator mi_it = name_map.find(*it);
		if(mi_it!=name_map.end())
		{
			const QModelIndex &mi = mi_it.value();
			ui->workspaceTreeView->selectionModel()->select(mi, QItemSelectionModel::Select);
		}
	}
}

//------------------------------------------------------------------------------
void MainWindow::updateFileView()
{
	// Clear content except headers
	getWorkspace().getFileModel().removeRows(0, getWorkspace().getFileModel().rowCount());

	struct { WorkspaceFile::Type type; QString text; const char *icon; }
	stats[] =
	{
		{   WorkspaceFile::TYPE_EDITTED, tr("Edited"), ":icons/icon-item-edited" },
		{   WorkspaceFile::TYPE_UNCHANGED, tr("Unchanged"), ":icons/icon-item-unchanged" },
		{   WorkspaceFile::TYPE_ADDED, tr("Added"), ":icons/icon-item-added" },
		{   WorkspaceFile::TYPE_DELETED, tr("Deleted"), ":icons/icon-item-deleted" },
		{   WorkspaceFile::TYPE_RENAMED, tr("Renamed"), ":icons/icon-item-renamed" },
		{   WorkspaceFile::TYPE_MISSING, tr("Missing"), ":icons/icon-item-missing" },
		{   WorkspaceFile::TYPE_CONFLICTED, tr("Conflicted"), ":icons/icon-item-conflicted" },
		{   WorkspaceFile::TYPE_MERGED, tr("Merged"), ":icons/icon-item-edited" },
	};

	bool display_path = viewMode==VIEWMODE_LIST || selectedDirs.count() > 1;

	const QString &status_unknown = QString(tr("Unknown"));
	const QString &search_text = searchBox->text();

	size_t item_id=0;
	for(Workspace::filemap_t::iterator it = getWorkspace().getFiles().begin(); it!=getWorkspace().getFiles().end(); ++it)
	{
		const WorkspaceFile &e = *it.value();
		const QString &path = e.getPath();
		const QString &file_path = e.getFilePath();
		QString native_file_path = QDir::toNativeSeparators(file_path);

		// Apply filter if available
		if(!search_text.isEmpty() && !native_file_path.contains(search_text, Qt::CaseInsensitive))
			continue;

		// In Tree mode, filter all items not included in the current dir
		if(viewMode==VIEWMODE_TREE && !selectedDirs.contains(path))
			continue;

		// Status Column
		const QString *status_text = &status_unknown;
		const char *status_icon_path= ":icons/icon-item-unknown"; // Default icon

		for(size_t t=0; t<COUNTOF(stats); ++t)
		{
			if(e.getType() == stats[t].type)
			{
				status_text = &stats[t].text;
				status_icon_path = stats[t].icon;
				break;
			}
		}

		QStandardItem *status = new QStandardItem(getCachedIcon(status_icon_path), *status_text);
		status->setToolTip(*status_text);
		getWorkspace().getFileModel().setItem(item_id, COLUMN_STATUS, status);

		QFileInfo finfo = e.getFileInfo();

		const QIcon *icon = &getCachedFileIcon(finfo);

		QStandardItem *filename_item = 0;
		getWorkspace().getFileModel().setItem(item_id, COLUMN_PATH, new QStandardItem(path));

		if(display_path)
			filename_item = new QStandardItem(*icon, native_file_path);
		else
			filename_item = new QStandardItem(*icon, e.getFilename());

		Q_ASSERT(filename_item);
		// Keep the path in the user data
		filename_item->setData(file_path);
		getWorkspace().getFileModel().setItem(item_id, COLUMN_FILENAME, filename_item);

		getWorkspace().getFileModel().setItem(item_id, COLUMN_EXTENSION, new QStandardItem(finfo.suffix()));
		getWorkspace().getFileModel().setItem(item_id, COLUMN_MODIFIED, new QStandardItem(finfo.lastModified().toString(Qt::SystemLocaleShortDate)));

		++item_id;
	}

	ui->fileTableView->resizeRowsToContents();
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
void MainWindow::applySettings()
{
	QSettings *store = settings.GetStore();
	QString active_workspace;

	int num_wks = store->beginReadArray("Workspaces");
	for(int i=0; i<num_wks; ++i)
	{
		store->setArrayIndex(i);
		QString wk = store->value("Path").toString();

		// Skip invalid workspaces
		if(wk.isEmpty() || !QDir(wk).exists())
			continue;

		addWorkspaceHistory(wk);

		if(store->contains("Active") && store->value("Active").toBool())
			active_workspace = wk;
	}
	store->endArray();

	store->beginReadArray("FileColumns");
	for(int i=0; i<getWorkspace().getFileModel().columnCount(); ++i)
	{
		store->setArrayIndex(i);
		if(store->contains("Width"))
		{
			int width = store->value("Width").toInt();
			ui->fileTableView->setColumnWidth(i, width);
		}

		if(store->contains("Index"))
		{
			int index = store->value("Index").toInt();
			int cur_index = ui->fileTableView->horizontalHeader()->visualIndex(i);
			ui->fileTableView->horizontalHeader()->moveSection(cur_index, index);
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
		ui->actionViewAsFolders->setChecked(!store->value("ViewAsList").toBool());
		viewMode = store->value("ViewAsList").toBool()? VIEWMODE_LIST : VIEWMODE_TREE;
	}

	// Set the workspace after loading the settings, since it may trigger a remote info storage
	if(!active_workspace.isEmpty())
		setCurrentWorkspace(active_workspace);


	// Custom Actions
	for(int i=0; i<MAX_CUSTOM_ACTIONS; ++i)
		settings.GetCustomActions()[i].Clear();

	int num_actions = store->beginReadArray("CustomActions");
	int last_action = 0;
	for(int i=0; i<num_actions; ++i)
	{
		store->setArrayIndex(i);
		CustomAction &action = settings.GetCustomActions()[last_action];

		QString descr;
		if(store->contains("Description"))
			descr = store->value("Description").toString();

		if(descr.isEmpty())
			continue;
		action.Description = descr;

		if(store->contains("Command"))
			action.Command = store->value("Command").toString();
		if(store->contains("Context"))
			action.Context = static_cast<CustomActionContext>(store->value("Context").toInt());
		if(store->contains("MultipleSelection"))
			action.MultipleSelection = store->value("MultipleSelection").toBool();

		++last_action;
	}
	store->endArray();

	updateCustomActions();
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
		if(getWorkspace().getPath() == workspaceHistory[i])
			store->setValue("Active", true);
		else
			store->remove("Active");
	}
	store->endArray();

	store->beginWriteArray("FileColumns", getWorkspace().getFileModel().columnCount());
	for(int i=0; i<getWorkspace().getFileModel().columnCount(); ++i)
	{
		store->setArrayIndex(i);
		store->setValue("Width", ui->fileTableView->columnWidth(i));
		int index = ui->fileTableView->horizontalHeader()->visualIndex(i);
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

	// Custom Actions
	Settings::custom_actions_t &actions = settings.GetCustomActions();
	store->beginWriteArray("CustomActions", actions.size());
	int active_actions = 0;
	for(int i=0; i<actions.size(); ++i)
	{
		CustomAction &action = actions[i];
		if(!action.IsValid())
			continue;
		store->setArrayIndex(active_actions);

		store->setValue("Description", action.Description);
		store->setValue("Command", action.Command);
		store->setValue("Context", static_cast<int>(action.Context));
		store->setValue("MultipleSelection", action.MultipleSelection);
		++active_actions;
	}
	store->endArray();
}

//------------------------------------------------------------------------------
void MainWindow::selectRootDir()
{
	// FIXME: KKK
	if(viewMode==VIEWMODE_TREE)
	{
		QModelIndex root_index = ui->workspaceTreeView->model()->index(0, 0);
		ui->workspaceTreeView->selectionModel()->select(root_index, QItemSelectionModel::Select);
	}
}

//------------------------------------------------------------------------------
void MainWindow::fossilBrowse(const QString &fossilUrl)
{
	if(!uiRunning())
		ui->actionFossilUI->activate(QAction::Trigger);

	bool use_internal = settings.GetValue(FUEL_SETTING_WEB_BROWSER).toInt() == 1;

	QUrl url = QUrl(getWorkspace().fossil().getUIHttpAddress()+fossilUrl);

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
	if(QApplication::focusWidget() == ui->workspaceTreeView)
		getDirViewSelection(filenames, includeMask, allIfEmpty);
	else
		getFileViewSelection(filenames, includeMask, allIfEmpty);
}

//------------------------------------------------------------------------------
void MainWindow::getSelectionPaths(stringset_t &paths)
{
	// Determine the directories selected
	QModelIndexList selection = ui->workspaceTreeView->selectionModel()->selectedIndexes();
	foreach(const QModelIndex &mi, selection)
	{
		QVariant data = mi.model()->data(mi, ROLE_WORKSPACE_ITEM);
		Q_ASSERT(data.isValid());

		WorkspaceItem tv = data.value<WorkspaceItem>();
		if(tv.Type != WorkspaceItem::TYPE_FOLDER && tv.Type != WorkspaceItem::TYPE_WORKSPACE)
			continue;

		paths.insert(tv.Value);
	}
}
//------------------------------------------------------------------------------
// Select all workspace files that match the includeMask
void MainWindow::getAllFilenames(QStringList &filenames, int includeMask)
{
	for(Workspace::filemap_t::iterator it=getWorkspace().getFiles().begin(); it!=getWorkspace().getFiles().end(); ++it)
	{
		const WorkspaceFile &e = *(*it);

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

	QModelIndexList selection = ui->workspaceTreeView->selectionModel()->selectedIndexes();
	if(!(selection.empty() && allIfEmpty))
	{
		getSelectionPaths(paths);
	}

	// Select the actual files form the selected directories
	for(Workspace::filemap_t::iterator it=getWorkspace().getFiles().begin(); it!=getWorkspace().getFiles().end(); ++it)
	{
		const WorkspaceFile &e = *(*it);

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
	QModelIndexList selection = ui->fileTableView->selectionModel()->selectedIndexes();
	if(selection.empty() && allIfEmpty)
	{
		ui->fileTableView->selectAll();
		selection = ui->fileTableView->selectionModel()->selectedIndexes();
		ui->fileTableView->clearSelection();
	}

	for(QModelIndexList::iterator mi_it = selection.begin(); mi_it!=selection.end(); ++mi_it)
	{
		const QModelIndex &mi = *mi_it;

		// FIXME: we are being called once per cell of each row
		// but we only need column 1. There must be a better way
		if(mi.column()!=COLUMN_FILENAME)
			continue;

		QVariant data = getWorkspace().getFileModel().data(mi, Qt::UserRole+1);
		QString filename = data.toString();
		Workspace::filemap_t::iterator e_it = getWorkspace().getFiles().find(filename);
		Q_ASSERT(e_it!=getWorkspace().getFiles().end());
		const WorkspaceFile &e = *e_it.value();

		// Skip unwanted files
		if(!(includeMask & e.getType()))
			continue;

		filenames.append(filename);
	}
}
//------------------------------------------------------------------------------
void MainWindow::getSelectionStashes(QStringList &stashNames)
{
	QModelIndexList selection = ui->workspaceTreeView->selectionModel()->selectedIndexes();

	foreach(const QModelIndex &mi, selection)
	{
		QVariant data = mi.model()->data(mi, ROLE_WORKSPACE_ITEM);
		Q_ASSERT(data.isValid());
		WorkspaceItem tv = data.value<WorkspaceItem>();

		if(tv.Type != WorkspaceItem::TYPE_STASH)
			continue;

		QString name = mi.model()->data(mi, Qt::DisplayRole).toString();
		stashNames.append(name);
	}
}

//------------------------------------------------------------------------------
void MainWindow::getSelectionRemotes(QStringList &remoteUrls)
{
	QModelIndexList selection = ui->workspaceTreeView->selectionModel()->selectedIndexes();

	foreach(const QModelIndex &mi, selection)
	{
		QVariant data = mi.model()->data(mi, ROLE_WORKSPACE_ITEM);
		Q_ASSERT(data.isValid());
		WorkspaceItem tv = data.value<WorkspaceItem>();

		if(tv.Type != WorkspaceItem::TYPE_REMOTE)
			continue;

		QString url = tv.Value;
		remoteUrls.append(url);
	}
}
//------------------------------------------------------------------------------
bool MainWindow::diffFile(const QString &repoFile)
{
	const QString &gdiff = settings.GetFossilValue(FOSSIL_SETTING_GDIFF_CMD).toString();
	if(!gdiff.isEmpty())
		return getWorkspace().fossil().diffFile(repoFile, true);
	else
		return getWorkspace().fossil().diffFile(repoFile, false);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionDiff_triggered()
{
	QStringList selection;
	getSelectionFilenames(selection, WorkspaceFile::TYPE_REPO);

	for(QStringList::iterator it = selection.begin(); it!=selection.end(); ++it)
		if(!diffFile(*it))
			return;
}

//------------------------------------------------------------------------------
bool MainWindow::startUI()
{
	bool started = getWorkspace().fossil().startUI("");
	ui->actionFossilUI->setChecked(started);
	return started;
}
//------------------------------------------------------------------------------
void MainWindow::stopUI()
{
	getWorkspace().fossil().stopUI();
	ui->webView->load(QUrl("about:blank"));
	ui->actionFossilUI->setChecked(false);
}

//------------------------------------------------------------------------------
bool MainWindow::uiRunning() const
{
	return getWorkspace().fossil().uiRunning();
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
void MainWindow::on_fileTableView_doubleClicked(const QModelIndex &/*index*/)
{
	int action = settings.GetValue(FUEL_SETTING_FILE_DBLCLICK).toInt();
	if(action==FILE_DLBCLICK_ACTION_DIFF)
		on_actionDiff_triggered();
	else if(action==FILE_DLBCLICK_ACTION_OPEN)
		on_actionOpenFile_triggered();
	else if(action==FILE_DLBCLICK_ACTION_OPENCONTAINING)
		on_actionOpenContaining_triggered();
	else if(action==FILE_DLBCLICK_ACTION_CUSTOM)
		invokeCustomAction(0);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionOpenFile_triggered()
{
	QStringList selection;
	getSelectionFilenames(selection);

	for(QStringList::iterator it = selection.begin(); it!=selection.end(); ++it)
	{
		QDesktopServices::openUrl(QUrl::fromLocalFile(getWorkspace().getPath()+QDir::separator()+*it));
	}
}

//------------------------------------------------------------------------------
void MainWindow::on_actionCommit_triggered()
{
	QStringList commit_files;
	getSelectionFilenames(commit_files, WorkspaceFile::TYPE_MODIFIED, true);

	if(commit_files.empty() && !getWorkspace().otherChanges())
		return;

	QStringList commit_msgs = settings.GetValue(FUEL_SETTING_COMMIT_MSG).toStringList();

	QString msg;
	QString branch_name="";
	bool private_branch = false;
	bool aborted = !CommitDialog::runCommit(this, commit_files, msg, commit_msgs, branch_name, private_branch);

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
	if(commit_files.empty() && !getWorkspace().otherChanges())
		return;

	// Do commit
	QStringList files;

	// When a subset of files has been selected, explicitely specify each file.
	// Otherwise all files will be implicitly committed by fossil. This is necessary
	// when committing after a merge where fossil thinks that we are trying to do
	// a partial commit which is not permitted.
	QStringList all_modified_files;
	getAllFilenames(all_modified_files, WorkspaceFile::TYPE_MODIFIED);

	if(commit_files.size() != all_modified_files.size())
		files = commit_files;

	if(!getWorkspace().fossil().commitFiles(files, msg, branch_name, private_branch))
		QMessageBox::critical(this, tr("Error"), tr("Could not commit changes."), QMessageBox::Ok);

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionAdd_triggered()
{
	// Get unknown files only
	QStringList selection;
	getSelectionFilenames(selection, WorkspaceFile::TYPE_UNKNOWN);

	if(selection.empty())
		return;

	if(!FileActionDialog::run(this, tr("Add files"), tr("The following files will be added.")+"\n"+tr("Are you sure?"), selection))
		return;

	// Do Add
	if(!getWorkspace().fossil().addFiles(selection))
		QMessageBox::critical(this, tr("Error"), tr("Could not add files."), QMessageBox::Ok);

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionDelete_triggered()
{
	QStringList repo_files;
	getSelectionFilenames(repo_files, WorkspaceFile::TYPE_REPO);

	QStringList unknown_files;
	getSelectionFilenames(unknown_files, WorkspaceFile::TYPE_UNKNOWN);

	QStringList all_files = repo_files+unknown_files;

	if(all_files.empty())
		return;

	bool remove_local = false;

	if(!FileActionDialog::run(this, tr("Remove files"), tr("The following files will be removed from the repository.")+"\n"+tr("Are you sure?"), all_files, tr("Also delete the local files"), &remove_local ))
		return;

	// Remove repository files
	if(!repo_files.empty())
	{
		if(!getWorkspace().fossil().removeFiles(repo_files, remove_local))
			QMessageBox::critical(this, tr("Error"), tr("Could not remove files."), QMessageBox::Ok);
	}

	// Remove unknown local files if selected
	if(remove_local)
	{
		for(int i=0; i<unknown_files.size(); ++i)
		{
			QFileInfo fi(getWorkspace().getPath() + QDir::separator() + unknown_files[i]);
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
	getSelectionFilenames(modified_files, WorkspaceFile::TYPE_MODIFIED);

	if(modified_files.empty())
		return;

	if(!FileActionDialog::run(this, tr("Revert files"), tr("The following files will be reverted.")+"\n"+tr("Are you sure?"), modified_files))
		return;

	// Do Revert
	if(!getWorkspace().fossil().revertFiles(modified_files))
		QMessageBox::critical(this, tr("Error"), tr("Could not revert files."), QMessageBox::Ok);

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionRename_triggered()
{
	QStringList repo_files;
	getSelectionFilenames(repo_files, WorkspaceFile::TYPE_REPO);

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
	if(!getWorkspace().fossil().renameFile(fi_before.filePath(), fi_after.filePath(), true))
		QMessageBox::critical(this, tr("Error"), tr("Could not rename file '%0' to '%1'").arg(fi_before.filePath(), fi_after.filePath()), QMessageBox::Ok);

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionOpenContaining_triggered()
{
	QStringList selection;
	getSelectionFilenames(selection);

	QString target;

	if(selection.empty())
		target = QDir::toNativeSeparators(getWorkspace().getPath());
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

	// Do test Undo
	if(!getWorkspace().fossil().undoWorkspace(res, true))
		QMessageBox::critical(this, tr("Error"), tr("Could not undo changes."), QMessageBox::Ok);

	if(res.length()>0 && res[0]=="No undo or redo is available")
		return;

	if(!FileActionDialog::run(this, tr("Undo"), tr("The following actions will be undone.")+"\n"+tr("Are you sure?"), res))
		return;

	// Do Undo
	if(!getWorkspace().fossil().undoWorkspace(res, false))
		QMessageBox::critical(this, tr("Error"), tr("Could not undo changes."), QMessageBox::Ok);

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionAbout_triggered()
{
	QString fossil_ver;
	getWorkspace().fossil().getExeVersion(fossil_ver);

	AboutDialog dlg(this, fossil_ver);
	dlg.exec();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionUpdate_triggered()
{
	QStringList selected = selectedBranches + selectedTags;

	QString revision;
	if(!selected.isEmpty())
		revision = selected.first();

	updateRevision(revision);
}

//------------------------------------------------------------------------------
void MainWindow::loadFossilSettings()
{
	// Also retrieve the fossil global settings
	QStringList out;

	if(!getWorkspace().fossil().getSettings(out))
		return;

	QStringMap kv;
	ParseProperties(kv, out);

	for(Settings::mappings_t::iterator it=settings.GetMappings().begin(); it!=settings.GetMappings().end(); ++it)
	{
		const QString &name = it.key();
		Settings::Setting::SettingType type = it->Type;

		Q_ASSERT(type == Settings::Setting::TYPE_FOSSIL_GLOBAL || type == Settings::Setting::TYPE_FOSSIL_LOCAL);
		Q_UNUSED(type);

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
	// Run the dialog
	if(!SettingsDialog::run(this, settings))
		return;

	getWorkspace().fossil().setExePath(settings.GetValue(FUEL_SETTING_FOSSIL_PATH).toString());
	updateCustomActions();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionFossilSettings_triggered()
{
	loadFossilSettings();

	// Run the dialog
	if(!FslSettingsDialog::run(this, settings))
		return;

	// Apply settings
	for(Settings::mappings_t::iterator it=settings.GetMappings().begin(); it!=settings.GetMappings().end(); ++it)
	{
		const QString &name = it.key();
		Settings::Setting::SettingType type = it.value().Type;

		Q_ASSERT(type == Settings::Setting::TYPE_FOSSIL_GLOBAL || type == Settings::Setting::TYPE_FOSSIL_LOCAL);

		QString value = it.value().Value.toString();
		getWorkspace().fossil().setSetting(name, value, type == Settings::Setting::TYPE_FOSSIL_GLOBAL);
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
void MainWindow::on_actionViewAll_triggered()
{
	ui->actionViewModified->setChecked(true);
	ui->actionViewUnchanged->setChecked(true);
	ui->actionViewUnknown->setChecked(true);
	ui->actionViewIgnored->setChecked(true);
	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionViewModifedOnly_triggered()
{
	ui->actionViewModified->setChecked(true);
	ui->actionViewUnchanged->setChecked(false);
	ui->actionViewUnknown->setChecked(false);
	ui->actionViewIgnored->setChecked(false);
	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionViewAsList_triggered()
{
	ui->actionViewAsFolders->setChecked(!ui->actionViewAsList->isChecked());
	viewMode = ui->actionViewAsList->isChecked() ? VIEWMODE_LIST : VIEWMODE_TREE;

	updateWorkspaceView();
	updateFileView();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionViewAsFolders_triggered()
{
	ui->actionViewAsList->setChecked(!ui->actionViewAsFolders->isChecked());
	viewMode = ui->actionViewAsList->isChecked() ? VIEWMODE_LIST : VIEWMODE_TREE;
	updateWorkspaceView();
	updateFileView();
}

//------------------------------------------------------------------------------
void MainWindow::onWorkspaceTreeViewSelectionChanged(const QItemSelection &/*selected*/, const QItemSelection &/*deselected*/)
{
	QModelIndexList indices = ui->workspaceTreeView->selectionModel()->selectedIndexes();

	// Do not modify the selection if nothing is selected
	if(indices.empty())
		return;

	stringset_t new_dirs;
	selectedTags.clear();
	selectedBranches.clear();

	foreach(const QModelIndex &id, indices)
	{
		QVariant data = id.model()->data(id, ROLE_WORKSPACE_ITEM);
		Q_ASSERT(data.isValid());
		WorkspaceItem tv = data.value<WorkspaceItem>();

		if(tv.Type == WorkspaceItem::TYPE_FOLDER || tv.Type == WorkspaceItem::TYPE_WORKSPACE)
			new_dirs.insert(tv.Value);
		else if(tv.Type == WorkspaceItem::TYPE_TAG)
			selectedTags.append(tv.Value);
		else if(tv.Type == WorkspaceItem::TYPE_BRANCH)
			selectedBranches.append(tv.Value);
	}

	// Update the selection if we have any new folders
	if(!new_dirs.empty() && viewMode == VIEWMODE_TREE)
	{
		selectedDirs = new_dirs;
		updateFileView();
	}
}

//------------------------------------------------------------------------------
void MainWindow::on_actionOpenFolder_triggered()
{
	const QItemSelection &selection =  ui->workspaceTreeView->selectionModel()->selection();

	if(selection.indexes().count()!=1)
		return;

	QModelIndex index = selection.indexes().at(0);
	on_workspaceTreeView_doubleClicked(index);
}

//------------------------------------------------------------------------------
void MainWindow::on_workspaceTreeView_doubleClicked(const QModelIndex &index)
{
	QVariant data = index.model()->data(index, ROLE_WORKSPACE_ITEM);
	Q_ASSERT(data.isValid());
	WorkspaceItem tv = data.value<WorkspaceItem>();

	if(tv.Type==WorkspaceItem::TYPE_FOLDER || tv.Type==WorkspaceItem::TYPE_WORKSPACE)
	{
		QString target = getWorkspace().getPath() + PATH_SEPARATOR + tv.Value;
		QUrl url = QUrl::fromLocalFile(target);
		QDesktopServices::openUrl(url);
	}
	else if(tv.Type==WorkspaceItem::TYPE_REMOTE)
	{
		on_actionEditRemote_triggered();
	}
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

	int dir_start = old_path.lastIndexOf(PATH_SEPARATOR);
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

	if(getWorkspace().getPaths().contains(new_path))
	{
		QMessageBox::critical(this, tr("Error"), tr("Cannot rename folder.")+"\n" +tr("This folder exists already."));
		return;
	}

	// Collect the files to be moved
	Workspace::filelist_t files_to_move;
	QStringList new_paths;
	QStringList operations;
	foreach(WorkspaceFile *r, getWorkspace().getFiles())
	{
		if(r->getPath().indexOf(old_path)!=0)
			continue;

		files_to_move.append(r);
		QString new_dir = new_path + r->getPath().mid(old_path.length());
		new_paths.append(new_dir);
		QString new_file_path =  new_dir + PATH_SEPARATOR + r->getFilename();
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
		WorkspaceFile *r = files_to_move[i];
		const QString &new_file_path = new_paths[i] + PATH_SEPARATOR + r->getFilename();

		if(!getWorkspace().fossil().renameFile(r->getFilePath(), new_file_path, false))
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
		QString target_path = QDir::cleanPath(getWorkspace().getPath() + PATH_SEPARATOR + new_paths[i] + PATH_SEPARATOR);
		QDir target(target_path);

		if(target.exists())
			continue;

		QDir wkdir(getWorkspace().getPath());
		Q_ASSERT(wkdir.exists());

		log(tr("Creating folder '%0'").arg(target_path)+"\n");
		if(!wkdir.mkpath(new_paths[i] + PATH_SEPARATOR + "."))
		{
			QMessageBox::critical(this, tr("Error"), tr("Cannot make target folder '%0'").arg(target_path));
			goto _exit;
		}
	}

	// Now that target directories exist copy files
	for(int i=0; i<files_to_move.length(); ++i)
	{
		WorkspaceFile *r = files_to_move[i];
		QString new_file_path = new_paths[i] + PATH_SEPARATOR + r->getFilename();

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
		WorkspaceFile *r = files_to_move[i];

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
const QIcon &MainWindow::getCachedIcon(const char* name)
{
	if(!iconCache.contains(name))
		iconCache.insert(name, QIcon(name));

	return iconCache[name];
}

//------------------------------------------------------------------------------
const QIcon &MainWindow::getCachedFileIcon(const QFileInfo &finfo)
{
	QString icon_type = iconProvider.type(finfo);

	// Exe files have varying icons, so key on path
	if(icon_type == "exe File")
		icon_type = finfo.absoluteFilePath();

	if(!iconCache.contains(icon_type))
		iconCache.insert(icon_type, iconProvider.icon(finfo));

	return iconCache[icon_type];
}
//------------------------------------------------------------------------------
void MainWindow::on_actionCreateStash_triggered()
{
	QStringList stashed_files;
	getSelectionFilenames(stashed_files, WorkspaceFile::TYPE_MODIFIED, true);

	if(stashed_files.empty())
		return;

	QString stash_name;
	bool revert = false;

	if(!CommitDialog::runStashNew(this, stashed_files, stash_name, revert) || stashed_files.empty())
		return;

	stash_name = stash_name.trimmed();

	if(stash_name.indexOf("\"")!=-1 || stash_name.isEmpty())
	{
		QMessageBox::critical(this, tr("Error"), tr("Invalid stash name"));
		return;
	}

	// Check that this stash does not exist
	for(stashmap_t::iterator it=getWorkspace().getStashes().begin(); it!=getWorkspace().getStashes().end(); ++it)
	{
		if(stash_name == it.key())
		{
			QMessageBox::critical(this, tr("Error"), tr("This stash already exists"));
			return;
		}
	}

	// Do Stash
	if(!getWorkspace().fossil().stashNew(stashed_files, stash_name, revert))
		QMessageBox::critical(this, tr("Error"), tr("Could not create stash."), QMessageBox::Ok);

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionApplyStash_triggered()
{
	QStringList stashes;
	getSelectionStashes(stashes);

	if(stashes.empty())
		return;

	bool delete_stashes = false;
	if(!FileActionDialog::run(this, tr("Apply Stash"), tr("The following stashes will be applied.")+"\n"+tr("Are you sure?"), stashes, tr("Delete after applying"), &delete_stashes))
		return;

	// Apply stashes
	for(QStringList::iterator it=stashes.begin(); it!=stashes.end(); ++it)
	{
		stashmap_t::iterator id_it = getWorkspace().getStashes().find(*it);
		Q_ASSERT(id_it!=getWorkspace().getStashes().end());

		if(!getWorkspace().fossil().stashApply(*id_it))
		{
			log(tr("Stash application aborted due to errors")+"\n");
			QMessageBox::critical(this, tr("Error"), tr("Could not apply stash."), QMessageBox::Ok);
			return;
		}
	}

	// Delete stashes
	for(QStringList::iterator it=stashes.begin(); delete_stashes && it!=stashes.end(); ++it)
	{
		stashmap_t::iterator id_it = getWorkspace().getStashes().find(*it);
		Q_ASSERT(id_it!=getWorkspace().getStashes().end());

		if(!getWorkspace().fossil().stashDrop(*id_it))
		{
			log(tr("Stash deletion aborted due to errors")+"\n");
			QMessageBox::critical(this, tr("Error"), tr("Could not delete stash."), QMessageBox::Ok);
			return;
		}
	}

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionDeleteStash_triggered()
{
	QStringList stashes;
	getSelectionStashes(stashes);

	if(stashes.empty())
		return;

	if(!FileActionDialog::run(this, tr("Delete Stashes"), tr("The following stashes will be deleted.")+"\n"+tr("Are you sure?"), stashes))
		return;

	// Delete stashes
	for(QStringList::iterator it=stashes.begin(); it!=stashes.end(); ++it)
	{
		stashmap_t::iterator id_it = getWorkspace().getStashes().find(*it);
		Q_ASSERT(id_it!=getWorkspace().getStashes().end());

		if(!getWorkspace().fossil().stashDrop(*id_it))
		{
			log(tr("Stash deletion aborted due to errors")+"\n");
			QMessageBox::critical(this, tr("Error"), tr("Could not delete stash."), QMessageBox::Ok);
			return;
		}
	}

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionDiffStash_triggered()
{
	QStringList stashes;
	getSelectionStashes(stashes);

	if(stashes.length() != 1)
		return;

	stashmap_t::iterator id_it = getWorkspace().getStashes().find(*stashes.begin());
	Q_ASSERT(id_it!=getWorkspace().getStashes().end());

	// Run diff
	if(!getWorkspace().fossil().stashDiff(*id_it))
		QMessageBox::critical(this, tr("Error"), tr("Could not diff stash."), QMessageBox::Ok);
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
		urls.append(QUrl::fromLocalFile(getWorkspace().getPath()+QDir::separator()+f));

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
void MainWindow::on_fileTableView_customContextMenuRequested(const QPoint &pos)
{
	QPoint gpos = QCursor::pos() + QPoint(1, 1);
#ifdef Q_OS_WIN
	if(qApp->keyboardModifiers() & Qt::SHIFT)
	{
		ui->fileTableView->selectionModel()->select(ui->fileTableView->indexAt(pos), QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
		QStringList fnames;
		getSelectionFilenames(fnames);

		if(fnames.size()==1)
		{
			QString fname = getWorkspace().getPath() + PATH_SEPARATOR + fnames[0];
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
		menu->addActions(ui->fileTableView->actions());
		menu->popup(gpos);
	}

}

//------------------------------------------------------------------------------
void MainWindow::on_workspaceTreeView_customContextMenuRequested(const QPoint &pos)
{
	ui->workspaceTreeView->selectionModel()->select(ui->workspaceTreeView->indexAt(pos), QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
	QModelIndexList indices = ui->workspaceTreeView->selectionModel()->selectedIndexes();

	if(indices.empty())
		return;

	QPoint gpos = QCursor::pos() + QPoint(1, 1);
	QMenu *menu = 0;

	// Get first selected item
	const QModelIndex &mi = indices.first();
	QVariant data = getWorkspace().getTreeModel().data(mi, ROLE_WORKSPACE_ITEM);
	Q_ASSERT(data.isValid());
	WorkspaceItem tv = data.value<WorkspaceItem>();

	if(tv.Type == WorkspaceItem::TYPE_FOLDER ||  tv.Type == WorkspaceItem::TYPE_WORKSPACE)
	{
	#ifdef Q_OS_WIN
		if(qApp->keyboardModifiers() & Qt::SHIFT)
		{
			QString fname = getWorkspace().getPath() + PATH_SEPARATOR + tv.Value;
			fname = QDir::toNativeSeparators(fname);
			ShowExplorerMenu((HWND)winId(), fname, gpos);
		}
		else
	#endif
		{
			menu = menuWorkspace;
		}
	}
	else if (tv.Type == WorkspaceItem::TYPE_STASH || tv.Type == WorkspaceItem::TYPE_STASHES)
		menu = menuStashes;
	else if (tv.Type == WorkspaceItem::TYPE_TAG || tv.Type == WorkspaceItem::TYPE_TAGS)
		menu = menuTags;
	else if (tv.Type == WorkspaceItem::TYPE_BRANCH || tv.Type == WorkspaceItem::TYPE_BRANCHES)
		menu = menuBranches;
	else if (tv.Type == WorkspaceItem::TYPE_REMOTE || tv.Type == WorkspaceItem::TYPE_REMOTES)
		menu = menuRemotes;

	if(menu)
		menu->popup(gpos);
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
			if(abspath.indexOf(getWorkspace().getPath())!=0)
				continue; // skip

			// Remove workspace from full path
			QString wkpath = abspath.right(abspath.length()-getWorkspace().getPath().length()-1);

			newfiles.append(wkpath);
		}

		// Any files to add?
		if(!newfiles.empty())
		{
			if(!FileActionDialog::run(this, tr("Add files"), tr("The following files will be added.")+"\n"+tr("Are you sure?"), newfiles))
				return;

			// Do Add
			if(!getWorkspace().fossil().addFiles(newfiles))
				QMessageBox::critical(this, tr("Error"), tr("Could not add files."), QMessageBox::Ok);

			refresh();
		}
	}
}

//------------------------------------------------------------------------------
void MainWindow::setBusy(bool busy)
{
	if(busy)
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	else
		QApplication::restoreOverrideCursor();

	ui->actionAbortOperation->setEnabled(busy);
	bool enabled = !busy;
	ui->menuBar->setEnabled(enabled);
	ui->mainToolBar->setEnabled(enabled);
	ui->centralWidget->setEnabled(enabled);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionAbortOperation_triggered()
{
	operationAborted = true;
	uiCallback.abortProcess();
	log("<br><b>* "+tr("Operation Aborted")+" *</b><br>", true);
}

//------------------------------------------------------------------------------
void MainWindow::fullRefresh()
{
	refresh();
	// Select the Root of the tree to update the file view
	selectRootDir();
}

//------------------------------------------------------------------------------
void MainWindow::MainWinUICallback::logText(const QString& text, bool isHTML)
{
	Q_ASSERT(mainWindow);
	mainWindow->log(text, isHTML);
}

//------------------------------------------------------------------------------
void MainWindow::MainWinUICallback::beginProcess(const QString& text)
{
	Q_ASSERT(mainWindow);
	aborted = false;
	mainWindow->ui->statusBar->showMessage(text);
	mainWindow->lblTags->setHidden(true);
	mainWindow->progressBar->setHidden(false);
	mainWindow->abortButton->setHidden(false);
	mainWindow->ui->actionAbortOperation->setEnabled(true);
	QCoreApplication::processEvents();
}

//------------------------------------------------------------------------------
void MainWindow::MainWinUICallback::updateProcess(const QString& text)
{
	Q_ASSERT(mainWindow);
	mainWindow->ui->statusBar->showMessage(text);
	QCoreApplication::processEvents();
}

//------------------------------------------------------------------------------
void MainWindow::MainWinUICallback::endProcess()
{
	Q_ASSERT(mainWindow);
	mainWindow->ui->statusBar->clearMessage();
	mainWindow->lblTags->setHidden(false);
	mainWindow->progressBar->setHidden(true);
	mainWindow->abortButton->setHidden(true);
	mainWindow->ui->actionAbortOperation->setEnabled(false);
	QCoreApplication::processEvents();
}

//------------------------------------------------------------------------------
QMessageBox::StandardButton MainWindow::MainWinUICallback::Query(const QString &title, const QString &query, QMessageBox::StandardButtons buttons)
{
	return DialogQuery(mainWindow, title, query, buttons);
}

//------------------------------------------------------------------------------
void MainWindow::updateRevision(const QString &revision)
{
	const QString latest = tr("<Latest Revision>");
	QString defaultval = latest;

	if(!revision.isEmpty())
		defaultval = revision;

	// Also include our "Latest Revision" to the version list
	QStringList versions = versionList;
	versions.push_front(latest);

	QString selected_revision = RevisionDialog::runUpdate(this, tr("Update workspace"), versions, defaultval).trimmed();

	// Nothing selected ?
	if(selected_revision.isEmpty())
		return;
	else if(selected_revision == latest)
		selected_revision = ""; // Empty revision is "latest"

	QStringList res;

	// Do test update
	if(!getWorkspace().fossil().updateWorkspace(res, selected_revision, true))
	{
		QMessageBox::critical(this, tr("Error"), tr("Could not update the repository."), QMessageBox::Ok);
		return;
	}

	if(res.length()==0)
		return;

	QStringMap kv;
	ParseProperties(kv, res, ':');

	// If no changes exit
	if(kv.contains("changes") && kv["changes"].indexOf("None.")!=-1)
		return;

	if(!FileActionDialog::run(this, tr("Update"), tr("The following files will be updated.")+"\n"+tr("Are you sure?"), res))
		return;

	// Do update
	if(!getWorkspace().fossil().updateWorkspace(res, selected_revision, false))
		QMessageBox::critical(this, tr("Error"), tr("Could not update the repository."), QMessageBox::Ok);

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionCreateTag_triggered()
{
	// Default to current revision
	QString revision = getWorkspace().getCurrentRevision();

	QString name;
	if(!RevisionDialog::runNewTag(this, tr("Create Tag"), versionList, revision, revision, name))
		return;

	if(name.isEmpty() || getWorkspace().getTags().contains(name) || getWorkspace().getBranches().contains(name))
	{
		QMessageBox::critical(this, tr("Error"), tr("Invalid name."), QMessageBox::Ok);
		return;
	}

	if(!getWorkspace().fossil().tagNew(name, revision))
		QMessageBox::critical(this, tr("Error"), tr("Could not create tag."), QMessageBox::Ok);

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionDeleteTag_triggered()
{
	if(selectedTags.size()!=1)
		return;

	const QString &tagname = selectedTags.first();

	if(QMessageBox::Yes != DialogQuery(this, tr("Delete Tag"), tr("Are you sure want to delete the tag '%0' ?").arg(tagname)))
		return;

	Q_ASSERT(getWorkspace().getTags().contains(tagname));

	const QString &revision = getWorkspace().getTags()[tagname];

	if(!getWorkspace().fossil().tagDelete(tagname, revision))
		QMessageBox::critical(this, tr("Error"), tr("Could not delete tag."), QMessageBox::Ok);

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionCreateBranch_triggered()
{
	// Default to current revision
	QString revision = getWorkspace().getCurrentRevision();

	QString branch_name;
	if(!RevisionDialog::runNewTag(this, tr("Create Branch"), versionList, revision, revision, branch_name))
		return;

	if(branch_name.isEmpty() || getWorkspace().getTags().contains(branch_name) || getWorkspace().getBranches().contains(branch_name))
	{
		QMessageBox::critical(this, tr("Error"), tr("Invalid name."), QMessageBox::Ok);
		return;
	}

	if(!getWorkspace().fossil().branchNew(branch_name, revision, false))
	{
		QMessageBox::critical(this, tr("Error"), tr("Could not create branch."), QMessageBox::Ok);
		return;
	}

	// Update to this branch.
	updateRevision(branch_name);
}
//------------------------------------------------------------------------------
void MainWindow::mergeRevision(const QString &defaultRevision)
{
	QStringList res;
	QString revision = defaultRevision;

	bool integrate = false;
	bool force = false;
	revision = RevisionDialog::runMerge(this, tr("Merge Branch"), versionList, revision, integrate, force);

	if(revision.isEmpty())
		return;

	// Do test merge
	if(!getWorkspace().fossil().branchMerge(res, revision, integrate, force, true))
	{
		QMessageBox::critical(this, tr("Error"), tr("Merge failed."), QMessageBox::Ok);
		return;
	}

	if(!FileActionDialog::run(this, tr("Merge"), tr("The following changes will be applied.")+"\n"+tr("Are you sure?"), res))
		return;

	// Do update
	if(!getWorkspace().fossil().branchMerge(res, revision, integrate, force, false))
		QMessageBox::critical(this, tr("Error"), tr("Merge failed."), QMessageBox::Ok);
	else
		log(tr("Merge completed. Don't forget to commit!")+"\n");

	refresh();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionMergeBranch_triggered()
{
	QString revision;

	if(!selectedBranches.isEmpty())
		revision = selectedBranches.first();
	mergeRevision(revision);
}

//------------------------------------------------------------------------------
void MainWindow::onSearchBoxTextChanged(const QString &)
{
	updateFileView();
}

//------------------------------------------------------------------------------
void MainWindow::onSearch()
{
	searchBox->selectAll();
	searchBox->setFocus();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionEditRemote_triggered()
{
	QStringList remotes;
	getSelectionRemotes(remotes);
	if(remotes.empty())
		return;

	QUrl old_url(remotes.first());

	QString name;
	Remote *remote = getWorkspace().findRemote(old_url);
	if(remote)
		name = remote->name;

	bool exists = KeychainGet(this, old_url, *settings.GetStore());

	QUrl new_url = old_url;
	if(!RemoteDialog::run(this, new_url, name))
		return;

	if(!new_url.isLocalFile())
	{
		if(exists)
			KeychainDelete(this, new_url, *settings.GetStore());

		if(!KeychainSet(this, new_url, *settings.GetStore()))
			QMessageBox::critical(this, tr("Error"), tr("Could not store information to keychain."), QMessageBox::Ok );
	}

	// Remove password
	new_url.setPassword("");
	old_url.setPassword("");
	// Url changed?
	if(new_url != old_url)
	{
		getWorkspace().removeRemote(old_url);
		getWorkspace().addRemote(new_url, name);
	}
	else // Just data changed
		remote->name = name;

	updateWorkspaceView();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionPushRemote_triggered()
{
	QStringList remotes;
	getSelectionRemotes(remotes);
	if(remotes.empty())
		return;

	QUrl url(remotes.first());

	// Retrieve password from keychain
	if(!url.isLocalFile())
		KeychainGet(this, url, *settings.GetStore());

	if(!getWorkspace().fossil().pushWorkspace(url))
		QMessageBox::critical(this, tr("Error"), tr("Could not push to the remote repository."), QMessageBox::Ok);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionPullRemote_triggered()
{
	QStringList remotes;
	getSelectionRemotes(remotes);
	if(remotes.empty())
		return;

	QUrl url(remotes.first());

	// Retrieve password from keychain
	if(!url.isLocalFile())
		KeychainGet(this, url, *settings.GetStore());

	if(!getWorkspace().fossil().pullWorkspace(url))
		QMessageBox::critical(this, tr("Error"), tr("Could not pull from the remote repository."), QMessageBox::Ok);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionPush_triggered()
{
	QUrl url = getWorkspace().getRemoteDefault();

	if(url.isEmpty())
	{
		QMessageBox::critical(this, tr("Error"), tr("A default remote repository has not been specified."), QMessageBox::Ok);
		return;
	}

	// Retrieve password from keychain
	if(!url.isLocalFile())
		KeychainGet(this, url, *settings.GetStore());

	if(!getWorkspace().fossil().pushWorkspace(url))
		QMessageBox::critical(this, tr("Error"), tr("Could not push to the remote repository."), QMessageBox::Ok);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionPull_triggered()
{
	QUrl url = getWorkspace().getRemoteDefault();

	if(url.isEmpty())
	{
		QMessageBox::critical(this, tr("Error"), tr("A default remote repository has not been specified."), QMessageBox::Ok);
		return;
	}

	// Retrieve password from keychain
	if(!url.isLocalFile())
		KeychainGet(this, url, *settings.GetStore());

	if(!getWorkspace().fossil().pullWorkspace(url))
		QMessageBox::critical(this, tr("Error"), tr("Could not pull from the remote repository."), QMessageBox::Ok);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionSetDefaultRemote_triggered()
{
	QStringList remotes;
	getSelectionRemotes(remotes);
	if(remotes.empty())
		return;

	QUrl url(remotes.first());

	if(getWorkspace().setRemoteDefault(url))
	{
		if(!url.isLocalFile())
			KeychainGet(this, url, *settings.GetStore());

		// FIXME: Fossil currently ignores the password in "remote-url"
		// which breaks commits due to a missing password when autosync is enabled
		// so only set the remote url when there is no password set
		if(url.password().isEmpty())
		{
			if(!getWorkspace().fossil().setRemoteUrl(url))
				QMessageBox::critical(this, tr("Error"), tr("Could not set the remote repository."), QMessageBox::Ok);
		}
	}

	updateWorkspaceView();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionAddRemote_triggered()
{
	QUrl url;
	QString name;
	if(!RemoteDialog::run(this, url, name))
		return;

	if(!url.isLocalFile())
	{
		KeychainDelete(this, url, *settings.GetStore());

		if(!KeychainSet(this, url, *settings.GetStore()))
			QMessageBox::critical(this, tr("Error"), tr("Could not store information to keychain."), QMessageBox::Ok);
	}

	url.setPassword("");

	getWorkspace().addRemote(url, name);
	updateWorkspaceView();
}

//------------------------------------------------------------------------------
void MainWindow::on_actionDeleteRemote_triggered()
{
	QStringList remotes;
	getSelectionRemotes(remotes);
	if(remotes.empty())
		return;

	QUrl url(remotes.first());

	Remote *remote = getWorkspace().findRemote(url);
	Q_ASSERT(remote);

	if(QMessageBox::Yes != DialogQuery(this, tr("Delete Remote"), tr("Are you sure want to delete the remote '%0' ?").arg(remote->name)))
		return;

	getWorkspace().removeRemote(url);
	updateWorkspaceView();
}

//------------------------------------------------------------------------------
void MainWindow::updateCustomActions()
{
	Settings::custom_actions_t custom_actions = settings.GetCustomActions();
	Q_ASSERT(MAX_CUSTOM_ACTIONS == custom_actions.size());

	// Remove All Actions
	for(int i = 0; i < custom_actions.size(); ++i)
	{
		QAction *action = customActions[i];
		ui->fileTableView->removeAction(action);
		menuWorkspace->removeAction(action);
	}
	ui->fileTableView->removeAction(fileActionSeparator);
	menuWorkspace->removeAction(workspaceActionSeparator);

	// Add them to the top
	ui->fileTableView->addAction(fileActionSeparator);
	menuWorkspace->addAction(workspaceActionSeparator);

	bool has_file_actions = false;
	bool has_folder_actions = false;

	for(int i = 0; i < custom_actions.size(); ++i)
	{
		CustomAction &cust_act = custom_actions[i];
		QAction *action = customActions[i];
		action->setVisible(cust_act.IsValid());
		action->setText(cust_act.Description);

		if(!cust_act.IsValid())
			continue;

		// Attempt to extract an icon
		QString cmd, extra_params;
		SplitCommandLine(cust_act.Command, cmd, extra_params);
		QFileInfo fi(cmd);
		if(fi.isFile())
			action->setIcon(getCachedFileIcon(fi));

		if(cust_act.IsActive(ACTION_CONTEXT_FILES))
		{
			ui->fileTableView->addAction(action);
			has_file_actions = true;
		}

		if(cust_act.IsActive(ACTION_CONTEXT_FOLDERS))
		{
			menuWorkspace->addAction(action);
			has_folder_actions = true;
		}
	}

	if(!has_file_actions)
		ui->fileTableView->removeAction(fileActionSeparator);

	if(!has_folder_actions)
		menuWorkspace->removeAction(workspaceActionSeparator);
}

//------------------------------------------------------------------------------
void MainWindow::invokeCustomAction(int actionId)
{
	Q_ASSERT(actionId < settings.GetCustomActions().size());
	CustomAction &cust_action = settings.GetCustomActions()[actionId];
	Q_ASSERT(cust_action.IsValid());

	Q_ASSERT(!cust_action.Command.isEmpty());

	QStringList file_selection;
	if(cust_action.IsActive(ACTION_CONTEXT_FILES))
		getSelectionFilenames(file_selection, WorkspaceFile::TYPE_ALL);

	stringset_t path_selection;
	if(cust_action.IsActive(ACTION_CONTEXT_FOLDERS))
		getSelectionPaths(path_selection);

	// Trim excess items for single selection
	if(!cust_action.MultipleSelection)
	{
		if(!file_selection.empty())
		{
			QString item = *file_selection.begin();
			file_selection.clear();
			file_selection.push_back(item);
			path_selection.clear();
		}
		else if(!path_selection.empty())
		{
			QString item = *path_selection.begin();
			path_selection.clear();
			path_selection.insert(item);
		}
	}

	const QString &wkdir = getWorkspace().getPath();

	SpawnExternalProcess(this, cust_action.Command, file_selection, path_selection, wkdir, uiCallback);
}

//------------------------------------------------------------------------------
void MainWindow::onCustomActionTriggered()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if(!action)
		return;

	int action_id = action->data().toInt();
	invokeCustomAction(action_id);
}

//------------------------------------------------------------------------------
void MainWindow::on_actionUpdateCheck_triggered()
{
	UpdateCheckDialog dlg(this);
	dlg.exec();
}
