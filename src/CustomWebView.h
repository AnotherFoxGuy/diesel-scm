#ifndef CUSTOMWEBVIEW_H
#define CUSTOMWEBVIEW_H

#include <QWebEngineView>

class CustomWebView : public QWebEngineView
{
    Q_OBJECT
public:
    explicit CustomWebView(QWidget *parent = 0);

signals:

public slots:

protected:
    virtual void mousePressEvent(QMouseEvent *event);
};

#endif  // CUSTOMWEBVIEW_H
