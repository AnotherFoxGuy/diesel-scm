#ifndef REMOTEDIALOG_H
#define REMOTEDIALOG_H

#include <QDialog>

namespace Ui {
class RemoteDialog;
}

class RemoteDialog : public QDialog
{
	Q_OBJECT

public:
	explicit RemoteDialog(QWidget *parent = 0);
	~RemoteDialog();

	static bool run(QWidget *parent, class QUrl &url);

private slots:
	void on_btnSelectSourceRepo_clicked();

private:
	void GetRepositoryPath(QString &pathResult);

	Ui::RemoteDialog *ui;
};

#endif // REMOTEDIALOG_H
