#include "LoggedProcess.h"

///////////////////////////////////////////////////////////////////////////////
LoggedProcess::LoggedProcess(QObject *parent) : QProcess(parent)
{
	setProcessChannelMode(QProcess::MergedChannels);
	connect(this, SIGNAL(readyReadStandardOutput()), this, SLOT(onReadyReadStandardOutput()));
}

void LoggedProcess::getLogAndClear(QByteArray &buffer)
{
	QMutexLocker lck(&mutex);
	buffer = log;
	log.clear();
}

void LoggedProcess::onReadyReadStandardOutput()
{
	QMutexLocker lck(&mutex);
	log.append(readAllStandardOutput());
}

