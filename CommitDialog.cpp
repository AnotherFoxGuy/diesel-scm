#include "CommitDialog.h"
#include "ui_CommitDialog.h"

CommitDialog::CommitDialog(const QStringList &commitMsgHistory, const QStringList &files, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CommitDialog)
{
    ui->setupUi(this);
	ui->comboBox->clear();
	ui->comboBox->insertItems(0, commitMsgHistory);
	ui->plainTextEdit->clear();
	ui->listView->setModel(&itemModel);

	for(QStringList::const_iterator it=files.begin(); it!=files.end(); ++it)
		itemModel.appendRow(new QStandardItem(*it));

}

CommitDialog::~CommitDialog()
{
	delete ui;
}

bool CommitDialog::run(QString &commitMsg, const QStringList &commitMsgHistory, const QStringList &files, QWidget *parent)
{
	CommitDialog dlg(commitMsgHistory, files, parent);
	int res = dlg.exec();
	if(res==QDialog::Accepted)
	{
		commitMsg = dlg.ui->plainTextEdit->toPlainText();
		return true;
	}

	return false;
}

void CommitDialog::on_comboBox_activated(const QString &arg1)
{
	ui->plainTextEdit->setPlainText(arg1);
}
