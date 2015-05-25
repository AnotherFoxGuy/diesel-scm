#include "FslSettingsDialog.h"
#include "ui_FslSettingsDialog.h"
#include "Utils.h"

#include <QDir>

///////////////////////////////////////////////////////////////////////////////
FslSettingsDialog::FslSettingsDialog(QWidget *parent, Settings &_settings) :
	QDialog(parent, Qt::Sheet),
	ui(new Ui::FslSettingsDialog),
	settings(&_settings)
{
	ui->setupUi(this);

	ui->lineUIPort->setText(settings->GetValue(FOSSIL_SETTING_HTTP_PORT).toString());

	// Global Settings
	ui->lineGDiffCommand->setText(settings->GetFossilValue(FOSSIL_SETTING_GDIFF_CMD).toString());
	ui->lineGMergeCommand->setText(settings->GetFossilValue(FOSSIL_SETTING_GMERGE_CMD).toString());
	ui->lineProxy->setText(settings->GetFossilValue(FOSSIL_SETTING_PROXY_URL).toString());

	// Repository Settings
	ui->lineIgnore->setText(settings->GetFossilValue(FOSSIL_SETTING_IGNORE_GLOB).toString());
	ui->lineIgnoreCRNL->setText(settings->GetFossilValue(FOSSIL_SETTING_CRNL_GLOB).toString());
}

//-----------------------------------------------------------------------------
FslSettingsDialog::~FslSettingsDialog()
{
	delete ui;
}

//-----------------------------------------------------------------------------
bool FslSettingsDialog::run(QWidget *parent, Settings &settings)
{
	FslSettingsDialog dlg(parent, settings);
	return dlg.exec() == QDialog::Accepted;
}

//-----------------------------------------------------------------------------
void FslSettingsDialog::on_buttonBox_accepted()
{
	settings->SetValue(FOSSIL_SETTING_HTTP_PORT, ui->lineUIPort->text());

	settings->SetFossilValue(FOSSIL_SETTING_GDIFF_CMD, ui->lineGDiffCommand->text());
	settings->SetFossilValue(FOSSIL_SETTING_GMERGE_CMD, ui->lineGMergeCommand->text());
	settings->SetFossilValue(FOSSIL_SETTING_PROXY_URL, ui->lineProxy->text());

	settings->SetFossilValue(FOSSIL_SETTING_IGNORE_GLOB, ui->lineIgnore->text());
	settings->SetFossilValue(FOSSIL_SETTING_CRNL_GLOB, ui->lineIgnoreCRNL->text());

	settings->ApplyEnvironment();
}

//-----------------------------------------------------------------------------
void FslSettingsDialog::on_btnSelectFossilGDiff_clicked()
{
	QString path = SelectExe(this, tr("Select Graphical Diff application"));
	if(!path.isEmpty())
		ui->lineGDiffCommand->setText(QDir::toNativeSeparators(path));
}

//-----------------------------------------------------------------------------
void FslSettingsDialog::on_btnSelectGMerge_clicked()
{
	QString path = SelectExe(this, tr("Select Graphical Merge application"));
	if(!path.isEmpty())
		ui->lineGMergeCommand->setText(path);
}

