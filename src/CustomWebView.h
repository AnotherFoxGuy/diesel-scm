#ifndef CUSTOMWEBVIEW_H
#define CUSTOMWEBVIEW_H

#include <QWebView>

class CustomWebView : public QWebView
{
	Q_OBJECT
public:
	explicit CustomWebView(QWidget *parent = 0);

signals:

public slots:

protected:
	virtual void mousePressEvent(QMouseEvent *event);

};

#endif // CUSTOMWEBVIEW_H
