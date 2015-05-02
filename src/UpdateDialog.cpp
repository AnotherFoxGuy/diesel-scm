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
QString UpdateDialog::runUpdate(QWidget *parent, const QString &title, const QStringList &completions, const QString &defaultValue)
{
	UpdateDialog dlg(parent, completions, defaultValue);
	dlg.setWindowTitle(title);
	dlg.ui->label_3->setVisible(false);
	dlg.ui->lineName->setVisible(false);

	if(dlg.exec() != QDialog::Accepted)
		return QString("");
	return dlg.ui->cmbRevision->currentText().trimmed();
}

//-----------------------------------------------------------------------------
bool UpdateDialog::runNewTag(QWidget *parent, const QString &title, const QStringList &completions, const QString &defaultValue, QString &revision, QString &name)
{
	UpdateDialog dlg(parent, completions, defaultValue);
	dlg.setWindowTitle(title);

	if(dlg.exec() != QDialog::Accepted)
		return false;

	revision = dlg.ui->cmbRevision->currentText().trimmed();
	name = dlg.ui->lineName->text().trimmed();
	return true;
}
