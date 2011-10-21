#include "CommitDialog.h"
#include <QPushButton>
#include "ui_CommitDialog.h"
#include "MainWindow.h" // Ugly. I know.

CommitDialog::CommitDialog(QWidget *parent, const QStringList &commitMsgHistory, QStringList &files) :
	QDialog(parent, Qt::Sheet),
    ui(new Ui::CommitDialog)
{
	ui->setupUi(this);
	ui->plainTextEdit->clear();
	ui->listView->setModel(&itemModel);

	// Generate the history combo
	foreach(const QString msg, commitMsgHistory)
	{
		QString trimmed = msg.trimmed();
		if(trimmed.isEmpty())
			continue;

		commitMessages.append(trimmed);
		QStringList lines = trimmed.split('\n');
		QString first_line;
		if(!lines.empty())
			first_line = lines[0] + "...";

		ui->comboBox->addItem(first_line);
	}

	// Populate file list
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
	commitMsg = dlg.ui->plainTextEdit->toPlainText();

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

	return true;
}

//------------------------------------------------------------------------------
void CommitDialog::on_comboBox_activated(int index)
{
	Q_ASSERT(index < commitMessages.length());
	ui->plainTextEdit->setPlainText(commitMessages[index]);
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
