#include "FileActionDialog.h"
#include "ui_FileActionDialog.h"

FileActionDialog::FileActionDialog(QWidget *parent, const QString &title, const QString &message, const QStringList &files, const QString &checkBoxText, bool *checkBoxResult) :
	QDialog(parent, Qt::Sheet),
    ui(new Ui::FileActionDialog)
{
    ui->setupUi(this);
	setWindowTitle(title);
	ui->label->setText(message);
	ui->listView->setModel(&itemModel);

	if(!checkBoxText.isEmpty() && checkBoxResult)
	{
		checkBox = new QCheckBox(this);
		checkBox->setText(checkBoxText);
		checkBox->setEnabled(true);
		checkBox->setChecked(*checkBoxResult);
		this->checkBoxResult = checkBoxResult;
		ui->verticalLayout->insertWidget(2, checkBox);
	}


	for(QStringList::const_iterator it=files.begin(); it!=files.end(); ++it)
		itemModel.appendRow(new QStandardItem(*it));
}

FileActionDialog::~FileActionDialog()
{
	delete ui;
}

bool FileActionDialog::run(QWidget *parent, const QString &title, const QString &message, const QStringList &files, const QString &checkBoxText, bool *checkBoxResult)
{
	FileActionDialog dlg(parent, title, message, files, checkBoxText, checkBoxResult);
	int res = dlg.exec();

	if(!checkBoxText.isEmpty() && checkBoxResult && dlg.checkBox)
		*checkBoxResult = dlg.checkBox->isChecked();

	return res == QDialog::Accepted;
}
