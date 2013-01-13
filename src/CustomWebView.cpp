#include "CustomWebView.h"
#include <QMouseEvent>

CustomWebView::CustomWebView(QWidget *parent) :
	QWebView(parent)
{
	setUrl(QUrl("about:blank"));
}

void CustomWebView::mousePressEvent(QMouseEvent *event)
{
	Qt::MouseButton but = event->button();
	if(but == Qt::XButton1)
		back();
	else if(but == Qt::XButton2)
		forward();
	else
		QWebView::mousePressEvent(event);
}
