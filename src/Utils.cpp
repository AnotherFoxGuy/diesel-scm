#include "Utils.h"
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QEventLoop>
#include <QUrl>
#include <QProcess>
#include <QCryptographicHash>
#include "ext/qtkeychain/keychain.h"

///////////////////////////////////////////////////////////////////////////////
QMessageBox::StandardButton DialogQuery(QWidget *parent, const QString &title, const QString &query, QMessageBox::StandardButtons buttons)
{
	QMessageBox mb(QMessageBox::Question, title, query, buttons, parent, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::Sheet );
	mb.setDefaultButton(QMessageBox::No);
	mb.setWindowModality(Qt::WindowModal);
	mb.setModal(true);
	mb.exec();
	QMessageBox::StandardButton res = mb.standardButton(mb.clickedButton());
	return res;
}

//-----------------------------------------------------------------------------
QString QuotePath(const QString &path)
{
	return path;
}

//-----------------------------------------------------------------------------
QStringList QuotePaths(const QStringList &paths)
{
	QStringList res;
	for(int i=0; i<paths.size(); ++i)
		res.append(QuotePath(paths[i]));
	return res;
}

//-----------------------------------------------------------------------------
QString SelectExe(QWidget *parent, const QString &description)
{
	QString filter(QObject::tr("Applications"));
#ifdef Q_OS_WIN
	filter += " (*.exe)";
#else
	filter += " (*)";
#endif
	QString path = QFileDialog::getOpenFileName(
				parent,
				description,
				QString(),
				filter,
				&filter);

	if(!QFile::exists(path))
		return QString();

	return path;
}
//-----------------------------------------------------------------------------
#if 0 // Unused
#include <QInputDialog>

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
#endif


#ifdef Q_OS_WIN
// Explorer File Context Menu support. Based on http://www.microsoft.com/msj/0497/wicked/wicked0497.aspx
#include <shlobj.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  FUNCTION:       DoExplorerMenu
//
//  DESCRIPTION:    Given a path name to a file or folder object, displays
//                  the shell's context menu for that object and executes
//                  the menu command (if any) selected by the user.
//
//  INPUT:          hwnd    = Handle of the window in which the menu will be
//                            displayed.
//
//                  pszPath = Pointer to an ANSI or Unicode string
//                            specifying the path to the object.
//
//                  point   = x and y coordinates of the point where the
//                            menu's upper left corner should be located, in
//                            client coordinates relative to hwnd.
//
//  RETURNS:        TRUE if successful, FALSE if not.
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool ShowExplorerMenu(HWND hwnd, const QString &path, const QPoint &qpoint)
{
	struct Util
	{
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		//  FUNCTION:       GetNextItem
		//  DESCRIPTION:    Finds the next item in an item ID list.
		//  INPUT:          pidl = Pointer to an item ID list.
		//  RETURNS:        Pointer to the next item.
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		static LPITEMIDLIST GetNextItem (LPITEMIDLIST pidl)
		{
			USHORT nLen;

			if ((nLen = pidl->mkid.cb) == 0)
				return NULL;

			return (LPITEMIDLIST) (((LPBYTE) pidl) + nLen);
		}

		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		//  FUNCTION:       GetItemCount
		//  DESCRIPTION:    Computes the number of item IDs in an item ID list.
		//  INPUT:          pidl = Pointer to an item ID list.
		//  RETURNS:        Number of item IDs in the list.
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		static UINT GetItemCount (LPITEMIDLIST pidl)
		{
			USHORT nLen;
			UINT nCount;

			nCount = 0;
			while ((nLen = pidl->mkid.cb) != 0) {
				pidl = GetNextItem (pidl);
				nCount++;
			}
			return nCount;
		}

		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		//  FUNCTION:       DuplicateItem
		//  DESCRIPTION:    Makes a copy of the next item in an item ID list.
		//  INPUT:          pMalloc = Pointer to an IMalloc interface.
		//                  pidl    = Pointer to an item ID list.
		//  RETURNS:        Pointer to an ITEMIDLIST containing the copied item ID.
		//  NOTES:          It is the caller's responsibility to free the memory
		//                  allocated by this function when the item ID is no longer
		//                  needed. Example:
		//                    pidlItem = DuplicateItem (pMalloc, pidl);
		//                      .
		//                      .
		//                      .
		//                    pMalloc->lpVtbl->Free (pMalloc, pidlItem);
		//                  Failure to free the ITEMIDLIST will result in memory
		//                  leaks.
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		static LPITEMIDLIST DuplicateItem (LPMALLOC pMalloc, LPITEMIDLIST pidl)
		{
			USHORT nLen;
			LPITEMIDLIST pidlNew;

			nLen = pidl->mkid.cb;
			if (nLen == 0)
				return NULL;

			pidlNew = (LPITEMIDLIST) pMalloc->Alloc (
				nLen + sizeof (USHORT));
			if (pidlNew == NULL)
				return NULL;

			CopyMemory (pidlNew, pidl, nLen);
			*((USHORT*) (((LPBYTE) pidlNew) + nLen)) = 0;

			return pidlNew;
		}
	};


	LPITEMIDLIST pidlMain, pidlItem, pidlNextItem, *ppidl;
	UINT nCount;
	POINT point;
	point.x = qpoint.x();
	point.y = qpoint.y();

	WCHAR wchPath[MAX_PATH];
	memset(wchPath, 0, sizeof(wchPath));
	Q_ASSERT(path.length()<MAX_PATH);
	path.toWCharArray(wchPath);

	//
	// Get pointers to the shell's IMalloc interface and the desktop's
	// IShellFolder interface.
	//
	bool bResult = false;

	LPMALLOC pMalloc;
	if (!SUCCEEDED (SHGetMalloc (&pMalloc)))
		return bResult;

	LPSHELLFOLDER psfFolder;
	if (!SUCCEEDED (SHGetDesktopFolder (&psfFolder))) {
		pMalloc->Release();
		return bResult;
	}

	//
	// Convert the path name into a pointer to an item ID list (pidl).
	//
	ULONG ulCount, ulAttr;
	if (SUCCEEDED (psfFolder->ParseDisplayName (hwnd,
		NULL, wchPath, &ulCount, &pidlMain, &ulAttr)) && (pidlMain != NULL)) {

		if ( (nCount = Util::GetItemCount (pidlMain))>0) { // nCount must be > 0
			//
			// Initialize psfFolder with a pointer to the IShellFolder
			// interface of the folder that contains the item whose context
			// menu we're after, and initialize pidlItem with a pointer to
			// the item's item ID. If nCount > 1, this requires us to walk
			// the list of item IDs stored in pidlMain and bind to each
			// subfolder referenced in the list.
			//
			pidlItem = pidlMain;

			while (--nCount) {
				//
				// Create a 1-item item ID list for the next item in pidlMain.
				//
				pidlNextItem = Util::DuplicateItem (pMalloc, pidlItem);
				if (pidlNextItem == NULL) {
					pMalloc->Free(pidlMain);
					psfFolder->Release ();
					pMalloc->Release ();
					return bResult;
				}

				//
				// Bind to the folder specified in the new item ID list.
				//
				LPSHELLFOLDER psfNextFolder;
				if (!SUCCEEDED (psfFolder->BindToObject (
					pidlNextItem, NULL, IID_IShellFolder, (void**)&psfNextFolder))) {
					pMalloc->Free (pidlNextItem);
					pMalloc->Free (pidlMain);
					psfFolder->Release ();
					pMalloc->Release ();
					return bResult;
				}

				//
				// Release the IShellFolder pointer to the parent folder
				// and set psfFolder equal to the IShellFolder pointer for
				// the current folder.
				//
				psfFolder->Release ();
				psfFolder = psfNextFolder;

				//
				// Release the storage for the 1-item item ID list we created
				// just a moment ago and initialize pidlItem so that it points
				// to the next item in pidlMain.
				//
				pMalloc->Free(pidlNextItem);
				pidlItem = Util::GetNextItem (pidlItem);
			}

			//
			// Get a pointer to the item's IContextMenu interface and call
			// IContextMenu::QueryContextMenu to initialize a context menu.
			//
			ppidl = &pidlItem;
			LPCONTEXTMENU pContextMenu;
			if (SUCCEEDED (psfFolder->GetUIObjectOf (
				hwnd, 1, (const ITEMIDLIST **)ppidl, IID_IContextMenu, NULL, (void**)&pContextMenu))) {

				HMENU hMenu = CreatePopupMenu ();
				if (SUCCEEDED (pContextMenu->QueryContextMenu (
					hMenu, 0, 1, 0x7FFF, CMF_EXPLORE))) {


					//
					// Display the context menu.
					//
					UINT nCmd = TrackPopupMenu (hMenu, TPM_LEFTALIGN |
						TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
						point.x, point.y, 0, hwnd, NULL);


					//
					// If a command was selected from the menu, execute it.
					//
					if (nCmd) {
						CMINVOKECOMMANDINFO ici;
						memset(&ici, 0, sizeof(ici) );
						ici.cbSize          = sizeof (CMINVOKECOMMANDINFO);
						ici.fMask           = 0;
						ici.hwnd            = hwnd;
						ici.lpVerb          = MAKEINTRESOURCEA (nCmd - 1);
						ici.lpParameters    = NULL;
						ici.lpDirectory     = NULL;
						ici.nShow           = SW_SHOWNORMAL;
						ici.dwHotKey        = 0;
						ici.hIcon           = NULL;

						if (SUCCEEDED (
							pContextMenu->InvokeCommand (
							(CMINVOKECOMMANDINFO*)&ici)))
							bResult = true;
					}
				}
				DestroyMenu (hMenu);
				pContextMenu->Release ();
			}
		}
		pMalloc->Free (pidlMain);
	}

	//
	// Clean up and return.
	//
	psfFolder->Release ();
	pMalloc->Release ();

	return bResult;
}

#endif

//-----------------------------------------------------------------------------
void ParseProperties(QStringMap &properties, const QStringList &lines, QChar separator)
{
	properties.clear();
	foreach(QString l, lines)
	{
		l = l.trimmed();
		int index = l.indexOf(separator);

		QString key;
		QString value;
		if(index!=-1)
		{
			key = l.left(index).trimmed();
			value = l.mid(index+1).trimmed();
		}
		else
			key = l;

		properties.insert(key, value);
	}
}


//------------------------------------------------------------------------------
void GetStandardItemTextRecursive(QString &name, const QStandardItem &item, const QChar &separator)
{
	if(item.parent())
	{
		GetStandardItemTextRecursive(name, *item.parent());
		name.append(separator);
	}

	name.append(item.data(Qt::DisplayRole).toString());
}

//------------------------------------------------------------------------------
void BuildNameToModelIndex(name_modelindex_map_t &map, const QStandardItem &item)
{
	QString name;
	GetStandardItemTextRecursive(name, item);
	map.insert(name, item.index());

	for(int i=0; i<item.rowCount(); ++i)
	{
		const QStandardItem *child = item.child(i);
		Q_ASSERT(child);
		BuildNameToModelIndex(map, *child);
	}
}

//------------------------------------------------------------------------------
void BuildNameToModelIndex(name_modelindex_map_t &map, const QStandardItemModel &model)
{
	for(int i=0; i<model.rowCount(); ++i)
	{
		const QStandardItem *item = model.item(i);
		Q_ASSERT(item);
		BuildNameToModelIndex(map, *item);
	}
}

//------------------------------------------------------------------------------
bool KeychainSet(QObject *parent, const QUrl &url, QSettings &settings)
{
	QEventLoop loop(parent);
	QKeychain::WritePasswordJob job(UrlToStringNoCredentials(url));

#ifdef Q_OS_WIN
	settings.beginGroup("Keychain");
	job.setSettings(&settings);
#else
	Q_UNUSED(settings)
#endif

	job.connect( &job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()) );
	job.setAutoDelete( false );
	job.setInsecureFallback(false);
	job.setKey(url.userName());
	job.setTextData(url.password());
	job.start();
	loop.exec();

#ifdef Q_OS_WIN
	settings.endGroup();
#endif

	return job.error() == QKeychain::NoError;
}

//------------------------------------------------------------------------------
bool KeychainGet(QObject *parent, QUrl &url, QSettings &settings)
{
	QEventLoop loop(parent);
	QKeychain::ReadPasswordJob job(UrlToStringNoCredentials(url));

#ifdef Q_OS_WIN
	settings.beginGroup("Keychain");
	job.setSettings(&settings);
#else
	Q_UNUSED(settings)
#endif

	job.connect( &job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()));
	job.setAutoDelete( false );
	job.setInsecureFallback(false);
	job.setAutoDelete( false );
	job.setKey(url.userName());
	job.start();
	loop.exec();

#ifdef Q_OS_WIN
	settings.endGroup();
#endif

	if(job.error() != QKeychain::NoError)
		return false;

	url.setUserName(job.key());
	url.setPassword(job.textData());
	return true;
}

//------------------------------------------------------------------------------
bool KeychainDelete(QObject* parent, const QUrl& url, QSettings &settings)
{
	QEventLoop loop(parent);
	QKeychain::DeletePasswordJob job(UrlToStringNoCredentials(url));

#ifdef Q_OS_WIN
	settings.beginGroup("Keychain");
	job.setSettings(&settings);
#else
	Q_UNUSED(settings)
#endif

	job.connect( &job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()));
	job.setAutoDelete( false );
	job.setInsecureFallback(false);
	job.setAutoDelete( false );
	job.setKey(url.userName());
	job.start();
	loop.exec();

#ifdef Q_OS_WIN
	settings.endGroup();
#endif

	return job.error() == QKeychain::NoError;
}

//------------------------------------------------------------------------------
QString HashString(const QString& str)
{
	QCryptographicHash hash(QCryptographicHash::Sha1);
	const QByteArray ba(str.toUtf8());
	hash.addData(ba.data(), ba.size());
	QString str_out(hash.result().toHex());
	return str_out;
}

//------------------------------------------------------------------------------
QString UrlToStringDisplay(const QUrl& url)
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
		return url.toString(QUrl::RemovePassword);
#else
		return url.toDisplayString();
#endif
}

//------------------------------------------------------------------------------
QString UrlToStringNoCredentials(const QUrl& url)
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
	return url.toString(QUrl::RemoveUserInfo);
#else
	return url.toString(QUrl::PrettyDecoded|QUrl::RemoveUserInfo);
#endif
}
//------------------------------------------------------------------------------
void SplitCommandLine(const QString &commandLine, QString &command, QString &extraParams)
{
	// Process command string
	command = commandLine;
	Q_ASSERT(!command.isEmpty());

	// Split command from embedded params
	extraParams.clear();

	// Command ends after first space
	QChar cmd_char_end = ' ';
	int start = 0;

	// ...unless it is a quoted command
	if(command[0]=='"')
	{
		cmd_char_end = '"';
		start = 1;
	}

	int cmd_end = command.indexOf(cmd_char_end, start);
	if(cmd_end != -1)
	{
		extraParams = command.mid(cmd_end+1);
		command = command.left(cmd_end);
	}

	command = command.trimmed();
	extraParams = extraParams.trimmed();
}

//------------------------------------------------------------------------------
bool SpawnExternalProcess(QObject *processParent, const QString& command, const QStringList& fileList, const stringset_t& pathSet, const QString &workspaceDir, UICallback &uiCallback)
{
	QStringList params;

	QString cmd, extra_params;
	SplitCommandLine(command, cmd, extra_params);

	// Push all additional params, except those containing macros
	QString macro_file;
	QString macro_folder;

	if(!extra_params.isEmpty())
	{
		QStringList extra_param_list = extra_params.split(' ');

		foreach(const QString &p, extra_param_list)
		{
			if(p.indexOf("$FILE")!=-1)
			{
				macro_file = p;
				continue;
			}
			else if(p.indexOf("$FOLDER")!=-1)
			{
				macro_folder = p;
				continue;
			}
			else if(p.indexOf("$WORKSPACE")!=-1)
			{
				// Add in-place
				QString n = p;
				n.replace("$WORKSPACE", workspaceDir, Qt::CaseInsensitive);
				params.push_back(n);
				continue;
			}

			params.push_back(p);
		}
	}

	// Build file params
	foreach(const QString &f, fileList)
	{
		QString path = QFileInfo(workspaceDir + PATH_SEPARATOR + f).absoluteFilePath();

		// Apply macro
		if(!macro_file.isEmpty())
		{
			QString macro = macro_file;
			path = macro.replace("$FILE", path, Qt::CaseInsensitive);
		}

		params.append(path);
	}


	// Build folder params
	foreach(const QString &f, pathSet)
	{
		QString path = QFileInfo(workspaceDir + PATH_SEPARATOR + f).absoluteFilePath();

		// Apply macro
		if(!macro_folder.isEmpty())
		{
			QString macro = macro_folder;
			path = macro.replace("$FOLDER", path, Qt::CaseInsensitive);
		}
		params.append(path);
	}

	// Skip action if nothing is available
	if(params.empty())
		return false;

	uiCallback.logText("<b>"+cmd + " "+params.join(" ")+"</b><br>", true);

	QProcess proc(processParent);
	return proc.startDetached(cmd, params);
}
