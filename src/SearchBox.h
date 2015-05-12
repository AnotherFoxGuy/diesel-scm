#ifndef SEARCHBOX_H
#define SEARCHBOX_H

#include <QLineEdit>

class SearchBox : public QLineEdit
{
	Q_OBJECT
public:
	explicit SearchBox(QWidget* parent=0);
	~SearchBox();

signals:

public slots:

protected:
	void keyPressEvent(QKeyEvent *event);

};


#endif // SEARCHBOX_H
