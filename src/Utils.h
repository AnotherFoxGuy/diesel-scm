#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QMessageBox>

QMessageBox::StandardButton DialogQuery(QWidget *parent, const QString &title, const QString &query, QMessageBox::StandardButtons buttons = QMessageBox::Yes|QMessageBox::No);

#ifdef Q_OS_WIN
	bool ShowExplorerMenu(HWND hwnd, const QString &path, const QPoint &qpoint);
#endif


#endif // UTILS_H
