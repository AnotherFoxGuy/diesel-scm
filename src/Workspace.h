#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <QStandardItemModel>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QMap>
#include <QSettings>
#include "Utils.h"
#include "Fossil.h"

//////////////////////////////////////////////////////////////////////////
// WorkspaceFile
//////////////////////////////////////////////////////////////////////////
struct WorkspaceFile
{
	enum Type
	{
		TYPE_UNKNOWN		= 1<<0,
		TYPE_UNCHANGED		= 1<<1,
		TYPE_EDITTED		= 1<<2,
		TYPE_ADDED			= 1<<3,
		TYPE_DELETED		= 1<<4,
		TYPE_MISSING		= 1<<5,
		TYPE_RENAMED		= 1<<6,
		TYPE_CONFLICTED		= 1<<7,
		TYPE_MERGED			= 1<<8,
		TYPE_MODIFIED		= TYPE_EDITTED|TYPE_ADDED|TYPE_DELETED|TYPE_MISSING|TYPE_RENAMED|TYPE_CONFLICTED|TYPE_MERGED,
		TYPE_REPO			= TYPE_UNCHANGED|TYPE_MODIFIED,
		TYPE_ALL			= TYPE_UNKNOWN|TYPE_REPO
	};

	WorkspaceFile(const QFileInfo &info, Type type, const QString &repoPath)
	{
		FileInfo = info;
		FileType = type;
		FilePath = getRelativeFilename(repoPath);
		Path = FileInfo.absolutePath();

		// Strip the workspace path from the path
		Q_ASSERT(Path.indexOf(repoPath)==0);
		Path = Path.mid(repoPath.length()+1);
	}

	bool isType(Type t) const
	{
		return FileType == t;
	}

	void setType(Type t)
	{
		FileType = t;
	}

	Type getType() const
	{
		return FileType;
	}

	QFileInfo getFileInfo() const
	{
		return FileInfo;
	}

	const QString &getFilePath() const
	{
		return FilePath;
	}

	QString getFilename() const
	{
		return FileInfo.fileName();
	}

	const QString &getPath() const
	{
		return Path;
	}

	QString getRelativeFilename(const QString &path)
	{
		QString abs_base_dir = QDir(path).absolutePath();

		QString relative = FileInfo.absoluteFilePath();
		int index = relative.indexOf(abs_base_dir);
		if(index<0)
			return QString("");

		return relative.right(relative.length() - abs_base_dir.length()-1);
	}

private:
	QFileInfo	FileInfo;
	Type		FileType;
	QString		FilePath;
	QString		Path;
};

class Remote
{
public:
	Remote(const QString &_name, const QUrl &_url, bool _isDefault=false)
	: name(_name), url(_url), isDefault(_isDefault)
	{
	}

	QString name;
	QUrl url;
	bool isDefault;

};

typedef QMap<QUrl, Remote> remote_map_t;
typedef QMap<QString, WorkspaceFile::Type> pathstate_map_t;


//////////////////////////////////////////////////////////////////////////
// Workspace
//////////////////////////////////////////////////////////////////////////
class Workspace
{
public:
	Workspace();

	~Workspace();

	typedef QList<WorkspaceFile*> filelist_t;
	typedef QMap<QString, WorkspaceFile*> filemap_t;

	void				clearState();

	Fossil &			fossil() { return bridge; }
	const Fossil &		fossil() const { return bridge; }

	bool				switchWorkspace(const QString &workspace, QSettings &store);
	void				scanWorkspace(bool scanLocal, bool scanIgnored, bool scanModified, bool scanUnchanged, const QString &ignoreGlob, UICallback &uiCallback, bool &operationAborted);

	QStandardItemModel	&getFileModel() { return repoFileModel; }
	QStandardItemModel	&getTreeModel() { return repoTreeModel; }

	filemap_t			&getFiles() { return workspaceFiles; }
	stringset_t			&getPaths() { return pathSet; }
	pathstate_map_t		&getPathState() { return pathState; }
	stashmap_t			&getStashes() { return stashMap; }
	QStringMap			&getTags() { return tags; }
	QStringList			&getBranches() { return branchList; }
	bool				otherChanges() const { return isIntegrated; }

	// Remotes
	const remote_map_t	&getRemotes() const { return remotes; }
	bool				addRemote(const QUrl &url, const QString &name);
	bool				removeRemote(const QUrl &url);
	bool				setRemoteDefault(const QUrl& url);
	const QUrl			&getRemoteDefault() const;
	Remote *			findRemote(const QUrl& url);


	void storeWorkspace(QSettings &store);
private:
	static bool			scanDirectory(QFileInfoList &entries, const QString& dirPath, const QString &baseDir, const QString ignoreSpec, const bool& abort, UICallback &uiCallback);

private:
	Fossil				bridge;
	filemap_t			workspaceFiles;
	stringset_t			pathSet;
	pathstate_map_t		pathState;
	stashmap_t			stashMap;
	QStringList			branchList;
	QStringMap			tags;
	remote_map_t		remotes;
	bool				isIntegrated;

	QStandardItemModel	repoFileModel;
	QStandardItemModel	repoTreeModel;
};

#endif // WORKSPACE_H
