#include "FileTableView.h"
#include <QMouseEvent>
#include <QApplication>

FileTableView::FileTableView(QWidget *parent) :
	QTableView(parent)
{
}

//------------------------------------------------------------------------------
void FileTableView::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
		dragStartPos = event->pos();

	QTableView::mousePressEvent(event);
}

//------------------------------------------------------------------------------
void FileTableView::mouseMoveEvent(QMouseEvent *event)
{
	int distance = (event->pos() - dragStartPos).manhattanLength();
	if (event->buttons() & Qt::LeftButton && distance >= QApplication::startDragDistance())
	{
		dragOutEvent();
		QTableView::mouseReleaseEvent(event);
	}
	else
		QTableView::mouseMoveEvent(event);

}
