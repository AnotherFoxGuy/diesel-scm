#include "CommitDialog.h"
#include <QPushButton>
#include "ui_CommitDialog.h"
#include "MainWindow.h" // Ugly. I know.

CommitDialog::CommitDialog(QWidget *parent, const QStringList &commitMsgHistory, QStringList &files) :
	QDialog(parent, Qt::Sheet),
    ui(new Ui::CommitDialog)
{
	ui->setupUi(this);
	ui->comboBox->clear();
	ui->comboBox->insertItems(0, commitMsgHistory);
	ui->plainTextEdit->clear();
	ui->listView->setModel(&itemModel);

	for(QStringList::const_iterator it=files.begin(); it!=files.end(); ++it)
	{
		QStandardItem *si = new QStandardItem(*it);
		si->setCheckable(true);
		si->setCheckState(Qt::Checked);
		itemModel.appendRow(si);
	}

}

//------------------------------------------------------------------------------
CommitDialog::~CommitDialog()
{
	delete ui;
}

//------------------------------------------------------------------------------
bool CommitDialog::run(QWidget *parent, QString &commitMsg, const QStringList &commitMsgHistory, QStringList &files)
{
	CommitDialog dlg(parent, commitMsgHistory, files);
	int res = dlg.exec();
	if(res!=QDialog::Accepted)
		return false;

	files.clear();
	for(int i=0; i<dlg.itemModel.rowCount(); ++i)
	{
		QStandardItem *si = dlg.itemModel.item(i);
		if(si->checkState()!=Qt::Checked)
			continue;
		files.append(si->text());
	}

	commitMsg = dlg.ui->plainTextEdit->toPlainText();
	return true;
}

//------------------------------------------------------------------------------
void CommitDialog::on_comboBox_activated(const QString &arg1)
{
	ui->plainTextEdit->setPlainText(arg1);
}


//------------------------------------------------------------------------------
void CommitDialog::on_listView_doubleClicked(const QModelIndex &index)
{
	QVariant data = itemModel.data(index);
	QString filename = data.toString();
	reinterpret_cast<MainWindow*>(parent())->diffFile(filename);
}

//------------------------------------------------------------------------------
void CommitDialog::on_listView_clicked(const QModelIndex &)
{
	int num_selected = 0;
	for(int i=0; i<itemModel.rowCount(); ++i)
	{
		QStandardItem *si = itemModel.item(i);
		if(si->checkState()==Qt::Checked)
			++num_selected;
	}

	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(num_selected>0);
}
