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
	explicit CommitDialog(QWidget *parent, const QStringList &commitMsgHistory, const QStringList &files);
    ~CommitDialog();

	static bool run(QWidget *parent, QString &commitMsg, const QStringList &commitMsgHistory, const QStringList &files);

private slots:
	void on_comboBox_activated(const QString &arg1);
	void on_plainTextEdit_textChanged();
	void on_listView_doubleClicked(const QModelIndex &index);

private:
    Ui::CommitDialog *ui;
	QStandardItemModel	itemModel;
};

#endif // COMMITDIALOG_H
