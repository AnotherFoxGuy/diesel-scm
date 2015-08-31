#ifndef UPDATECHECKDIALOG_H
#define UPDATECHECKDIALOG_H

#include <QDialog>
#include <QNetworkAccessManager>

namespace Ui {
class UpdateCheckDialog;
}

class UpdateCheckDialog : public QDialog
{
	Q_OBJECT

public:
	explicit UpdateCheckDialog(QWidget *parent);
	~UpdateCheckDialog();

private slots:
  void versionInfoDownloaded(QNetworkReply* reply);
  void changelogDownloaded(QNetworkReply* reply);

private:
	Ui::UpdateCheckDialog *ui;
	QNetworkAccessManager networkAccess;
};

#endif // UPDATECHECKDIALOG_H
