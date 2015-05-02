#include "UpdateDialog.h"
#include "ui_UpdateDialog.h"
#include "Utils.h"

//-----------------------------------------------------------------------------
UpdateDialog::UpdateDialog(QWidget *parent, const QStringList &completions, const QString &defaultValue) :
	QDialog(parent),
	ui(new Ui::UpdateDialog),
	completer(completions, parent)
{
	ui->setupUi(this);
	ui->cmbRevision->setCompleter(&completer);

	ui->cmbRevision->addItems(completions);

	if(!defaultValue.isEmpty())
		ui->cmbRevision->setCurrentText(defaultValue);
}

//-----------------------------------------------------------------------------
UpdateDialog::~UpdateDialog()
{
	delete ui;
}

//-----------------------------------------------------------------------------
QString UpdateDialog::run(QWidget *parent, const QStringList &completions, const QString &defaultValue)
{
	UpdateDialog dlg(parent, completions, defaultValue);

	if(dlg.exec() != QDialog::Accepted)
		return QString("");
	return dlg.ui->cmbRevision->currentText();
}
