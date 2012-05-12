#ifndef LOGGEDPROCESS_H
#define LOGGEDPROCESS_H

#include <QProcess>
#include <QMutex>

class LoggedProcess : public QProcess
{
	Q_OBJECT
public:
	explicit LoggedProcess(QObject *parent = 0);
	void getLogAndClear(QByteArray &buffer);
	bool isLogEmpty() const { return log.isEmpty(); }
	qint64 logBytesAvailable() const { return log.size(); }

private slots:
	void onReadyReadStandardOutput();

private:
	QMutex mutex;
	QByteArray log;
};

#endif // LOGGEDPROCESS_H
