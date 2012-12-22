#ifndef BROWSERWIDGET_H
#define BROWSERWIDGET_H

#include <QWidget>
#include <QUrl>

namespace Ui {
class BrowserWidget;
}

class BrowserWidget : public QWidget
{
	Q_OBJECT

public:
	explicit BrowserWidget(QWidget *parent = 0);
	~BrowserWidget();

	void load(const QUrl &url);

private slots:
	void on_webView_urlChanged(const QUrl &url);
	void on_webView_loadStarted();
	void on_webView_loadFinished(bool);
	void on_actionBrowserBack_triggered();
	void on_actionBrowserForward_triggered();
	void on_actionBrowserRefresh_triggered();
	void on_txtUrl_returnPressed();

	void on_actionBrowserStop_triggered();

private:
	Ui::BrowserWidget *ui;
	bool loading;
};

#endif // BROWSERWIDGET_H
