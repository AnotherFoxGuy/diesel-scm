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
	explicit CommitDialog(QWidget *parent, QString title, QStringList &files, const QStringList *history=0, bool singleLineEntry=false, const QString *checkBoxText=0, bool *checkBoxValue=0);
    ~CommitDialog();

	static bool run(QWidget *parent, QString title, QStringList &files, QString &commitMsg, const QStringList *history=0, bool singleLineEntry=false, const QString *checkBoxText=0, bool *checkBoxValue=0);

private slots:
	void on_comboBox_activated(int index);
	void on_listView_doubleClicked(const QModelIndex &index);
	void on_listView_clicked(const QModelIndex &index);

private:
    Ui::CommitDialog *ui;
	QStandardItemModel	itemModel;
	QStringList commitMessages;
};

#endif // COMMITDIALOG_H
