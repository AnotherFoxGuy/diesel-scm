#ifndef WORKSPACECOMMON_H
#define WORKSPACECOMMON_H

#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QMap>
#include <QUrl>
#include "Utils.h"

//////////////////////////////////////////////////////////////////////////
// WorkspaceState
//////////////////////////////////////////////////////////////////////////
enum WorkspaceState
{
	WORKSPACE_STATE_OK,
	WORKSPACE_STATE_NOTFOUND,
	WORKSPACE_STATE_OLDSCHEMA
};

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
typedef QList<WorkspaceFile*> filelist_t;
typedef QMap<QString, WorkspaceFile*> filemap_t;
typedef QMap<QString, QString> stashmap_t;


#endif // WORKSPACECOMMON_H
