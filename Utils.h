#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QWidget>

enum DialogAnswer
{
	ANSWER_YES,
	ANSWER_NO,
	ANSWER_YESALL
};

DialogAnswer DialogQuery(QWidget *parent, const QString &title, const QString &query, bool yesToAllButton=false);


#endif // UTILS_H
