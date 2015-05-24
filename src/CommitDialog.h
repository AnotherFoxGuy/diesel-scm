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
	explicit CommitDialog(QWidget *parent, const QString &title, QStringList &files, const QStringList *history, bool stashMode);
	~CommitDialog();

	static bool runStashNew(QWidget* parent, QStringList& stashedFiles, QString& stashName, bool &revertFiles);
	static bool runCommit(QWidget* parent, QStringList& files, QString& commitMsg, const QStringList &commitMsgHistory, QString& branchName, bool& privateBranch);

private slots:
	void on_comboBox_activated(int index);
	void on_listView_doubleClicked(const QModelIndex &index);
	void on_listView_clicked(const QModelIndex &index);
	void on_chkNewBranch_toggled(bool checked);

private:
	Ui::CommitDialog *ui;
	QStandardItemModel	itemModel;
	QStringList commitMessages;
};

#endif // COMMITDIALOG_H
