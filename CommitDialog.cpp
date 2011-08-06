#include "CommitDialog.h"
#include <QPushButton>
#include "ui_CommitDialog.h"
#include "MainWindow.h" // Ugly. I know.

CommitDialog::CommitDialog(QWidget *parent, const QStringList &commitMsgHistory, const QStringList &files) :
	QDialog(parent, Qt::Sheet),
    ui(new Ui::CommitDialog)
{
	setModal(true);
	setWindowModality(Qt::WindowModal);
	ui->setupUi(this);
	ui->comboBox->clear();
	ui->comboBox->insertItems(0, commitMsgHistory);
	ui->plainTextEdit->clear();
	ui->listView->setModel(&itemModel);

	for(QStringList::const_iterator it=files.begin(); it!=files.end(); ++it)
		itemModel.appendRow(new QStandardItem(*it));


}

//------------------------------------------------------------------------------
CommitDialog::~CommitDialog()
{
	delete ui;
}

//------------------------------------------------------------------------------
bool CommitDialog::run(QWidget *parent, QString &commitMsg, const QStringList &commitMsgHistory, const QStringList &files)
{
	CommitDialog dlg(parent, commitMsgHistory, files);
	int res = dlg.exec();
	if(res==QDialog::Accepted)
	{
		commitMsg = dlg.ui->plainTextEdit->toPlainText();
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
void CommitDialog::on_comboBox_activated(const QString &arg1)
{
	ui->plainTextEdit->setPlainText(arg1);
}

//------------------------------------------------------------------------------
void CommitDialog::on_plainTextEdit_textChanged()
{
	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!ui->plainTextEdit->toPlainText().isEmpty());
}

//------------------------------------------------------------------------------
void CommitDialog::on_listView_doubleClicked(const QModelIndex &index)
{
	QVariant data = itemModel.data(index);
	QString filename = data.toString();
	reinterpret_cast<MainWindow*>(parent())->diffFile(filename);
}
