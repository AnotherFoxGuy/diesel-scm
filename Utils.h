#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QMessageBox>

QMessageBox::StandardButton DialogQuery(QWidget *parent, const QString &title, const QString &query, QMessageBox::StandardButtons buttons = QMessageBox::Yes|QMessageBox::No);


#endif // UTILS_H
