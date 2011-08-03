#ifndef COMMITDIALOG_H
#define COMMITDIALOG_H

#include <QDialog>
#include <QStandardItemModel>

namespace Ui {
    class CommitDialog;
}

class CommitDialog : public QDialog
{
    Q_OBJECT

public:
	explicit CommitDialog(const QStringList &commitMsgHistory, const QStringList &files, QWidget *parent = 0);
    ~CommitDialog();

	static bool run(QString &commitMsg, const QStringList &commitMsgHistory, const QStringList &files, QWidget *parent);

private slots:
	void on_comboBox_activated(const QString &arg1);

private:
    Ui::CommitDialog *ui;
	QStandardItemModel	itemModel;
};

#endif // COMMITDIALOG_H
