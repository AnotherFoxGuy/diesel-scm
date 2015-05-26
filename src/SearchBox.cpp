#include "SearchBox.h"
#include <QKeyEvent>

SearchBox::SearchBox(QWidget *parent) : QLineEdit(parent)
{
}

SearchBox::~SearchBox()
{

}

void SearchBox::keyPressEvent(QKeyEvent *event)
{
	// Clear text on escape
	if(event->key() == Qt::Key_Escape)
	{
		setText("");
		clearFocus();
	}
	else
		QLineEdit::keyPressEvent(event);
}

