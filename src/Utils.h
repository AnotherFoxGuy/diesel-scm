#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QMessageBox>

#define COUNTOF(array)			(sizeof(array)/sizeof(array[0]))
#define FOSSIL_CHECKOUT1	"_FOSSIL_"
#define FOSSIL_CHECKOUT2	".fslckout"
#define FOSSIL_EXT			"fossil"


QMessageBox::StandardButton DialogQuery(QWidget *parent, const QString &title, const QString &query, QMessageBox::StandardButtons buttons = QMessageBox::Yes|QMessageBox::No);
QString						QuotePath(const QString &path);
QStringList					QuotePaths(const QStringList &paths);

#ifdef Q_OS_WIN
	bool ShowExplorerMenu(HWND hwnd, const QString &path, const QPoint &qpoint);
#endif

class UICallback
{
public:
	virtual void logText(const QString &text, bool isHTML)=0;
	virtual void beginProcess(const QString &text)=0;
	virtual void updateProcess(const QString &text)=0;
	virtual void endProcess()=0;
	virtual QMessageBox::StandardButton Query(const QString &title, const QString &query, QMessageBox::StandardButtons buttons)=0;
};


class ScopedStatus
{
public:
	ScopedStatus(UICallback *callback, const QString &text) : uiCallback(callback)
	{
		uiCallback->beginProcess(text);
	}

	~ScopedStatus()
	{
		uiCallback->endProcess();
	}

private:
	UICallback *uiCallback;
};

#endif // UTILS_H
