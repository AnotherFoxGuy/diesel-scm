#include <QLineEdit>
#include "BrowserWidget.h"
#include "ui_BrowserWidget.h"

BrowserWidget::BrowserWidget(QWidget *parent) :
	QWidget(parent),
	loading(false)
{
	ui = new Ui::BrowserWidget;
	ui->setupUi(this);
	ui->toolBar->addWidget(ui->txtUrl);
	connect(ui->txtUrl, SIGNAL(returnPressed()), this, SLOT(on_txtUrl_returnPressed()) );
	ui->actionBrowserStop->setVisible(false);
}

BrowserWidget::~BrowserWidget()
{
	delete ui;
}

void BrowserWidget::load(const QUrl &url)
{
	ui->webView->load(url);
}

void BrowserWidget::on_webView_urlChanged(const QUrl &url)
{
	ui->txtUrl->setText(url.toString());
}

void BrowserWidget::on_webView_loadStarted()
{
	loading=true;
	ui->actionBrowserRefresh->setVisible(false);
	ui->actionBrowserStop->setVisible(true);
}

void BrowserWidget::on_webView_loadFinished(bool /*errorOccured*/)
{
	loading=false;
	ui->actionBrowserRefresh->setVisible(true);
	ui->actionBrowserStop->setVisible(false);
}


void BrowserWidget::on_txtUrl_returnPressed()
{
	QUrl url(ui->txtUrl->text());
	if(url.scheme().isEmpty())
		url.setScheme("http");
	ui->webView->load(url);
}


void BrowserWidget::on_actionBrowserBack_triggered()
{
	ui->webView->back();
}

void BrowserWidget::on_actionBrowserForward_triggered()
{
	ui->webView->forward();
}

void BrowserWidget::on_actionBrowserRefresh_triggered()
{
	ui->webView->reload();
}

void BrowserWidget::on_actionBrowserStop_triggered()
{
	ui->webView->stop();
}
