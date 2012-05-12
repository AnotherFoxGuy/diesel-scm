#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QMessageBox>
#include <QProcess>
#include <QMutex>

QMessageBox::StandardButton DialogQuery(QWidget *parent, const QString &title, const QString &query, QMessageBox::StandardButtons buttons = QMessageBox::Yes|QMessageBox::No);

#ifdef Q_WS_WIN
	bool ShowExplorerMenu(HWND hwnd, const QString &path, const QPoint &qpoint);
#endif

class QLoggedProcess : public QProcess
{
	Q_OBJECT
public:
	explicit QLoggedProcess(QObject *parent = 0);
	void getLogAndClear(QByteArray &buffer);
	bool isLogEmpty() const { return log.isEmpty(); }
	qint64 logBytesAvailable() const { return log.size(); }

private slots:
	void onReadyReadStandardOutput();

private:
	QMutex mutex;
	QByteArray log;
};

#endif // UTILS_H
