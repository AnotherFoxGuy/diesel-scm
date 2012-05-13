#include <QCheckBox>
#include "FileActionDialog.h"
#include "ui_FileActionDialog.h"

FileActionDialog::FileActionDialog(QWidget *parent, const QString &title, const QString &message, const QStringList &listData, const QString &checkBoxText, bool *checkBoxResult) :
	QDialog(parent, Qt::Sheet),
	ui(new Ui::FileActionDialog),
	clickedButton(QDialogButtonBox::NoButton)
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

	if(listData.empty())
		ui->listView->setVisible(false);
	else
	{
		foreach(const QString &s, listData)
			itemModel.appendRow(new QStandardItem(s));
	}
}

FileActionDialog::~FileActionDialog()
{
	delete ui;
}

bool FileActionDialog::run(QWidget *parent, const QString &title, const QString &message, const QStringList &listData, const QString &checkBoxText, bool *checkBoxResult)
{
	FileActionDialog dlg(parent, title, message, listData, checkBoxText, checkBoxResult);
	int res = dlg.exec();

	if(!checkBoxText.isEmpty() && checkBoxResult && dlg.checkBox)
		*checkBoxResult = dlg.checkBox->isChecked();

	return res == QDialog::Accepted;
}

QDialogButtonBox::StandardButton FileActionDialog::runStandardButtons(QWidget *parent, StandardButtons buttons, const QString &title, const QString &message, const QStringList &listData)
{
	FileActionDialog dlg(parent, title, message, listData);
	dlg.ui->buttonBox->setStandardButtons(buttons);

	dlg.exec();
	return dlg.clickedButton;
}

void FileActionDialog::on_buttonBox_clicked(QAbstractButton *button)
{
	// Retrieve the flag corresponding to the standard clicked
	clickedButton = ui->buttonBox->standardButton(button);
}

