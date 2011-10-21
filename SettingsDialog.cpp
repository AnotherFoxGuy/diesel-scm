#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include <QFileDialog>
#include "Utils.h"

static QString SelectExe(QWidget *parent, const QString &description)
{
#ifdef Q_WS_WIN
	QString filter("Applications (*.exe)");
#else
	QString filter("Applications (*)");
#endif
	QString path = QFileDialog::getOpenFileName(
				parent,
				"Select "+description,
				QString(),
				filter,
				&filter);

	if(!QFile::exists(path))
		return QString();

    return path;
}

///////////////////////////////////////////////////////////////////////////////
SettingsDialog::SettingsDialog(QWidget *parent, Settings &_settings) :
	QDialog(parent, Qt::Sheet),
	ui(new Ui::SettingsDialog),
	settings(&_settings)
{
	ui->setupUi(this);
	ui->lineFossilPath->setText(QDir::toNativeSeparators(settings->Mappings[FUEL_SETTING_FOSSIL_PATH].Value.toString()));
	ui->lineGDiffCommand->setText(QDir::toNativeSeparators(settings->Mappings[FUEL_SETTING_GDIFF_CMD].Value.toString()));
	ui->lineGMergeCommand->setText(QDir::toNativeSeparators(settings->Mappings[FUEL_SETTING_GMERGE_CMD].Value.toString()));
	ui->lineRemoteURL->setText(settings->Mappings[FUEL_SETTING_REMOTE_URL].Value.toString());
	ui->lineIgnore->setText(settings->Mappings[FUEL_SETTING_IGNORE_GLOB].Value.toString());
	ui->lineIgnoreCRNL->setText(settings->Mappings[FUEL_SETTING_CRNL_GLOB].Value.toString());
}

//-----------------------------------------------------------------------------
SettingsDialog::~SettingsDialog()
{
    delete ui;
}

//-----------------------------------------------------------------------------
bool SettingsDialog::run(QWidget *parent, Settings &settings)
{
	SettingsDialog dlg(parent, settings);
	return dlg.exec() == QDialog::Accepted;
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_buttonBox_accepted()
{
	settings->Mappings[FUEL_SETTING_FOSSIL_PATH].Value = QDir::fromNativeSeparators(ui->lineFossilPath->text());
	settings->Mappings[FUEL_SETTING_GDIFF_CMD].Value = QDir::fromNativeSeparators(ui->lineGDiffCommand->text());
	settings->Mappings[FUEL_SETTING_GMERGE_CMD].Value = QDir::fromNativeSeparators(ui->lineGMergeCommand->text());
	settings->Mappings[FUEL_SETTING_REMOTE_URL].Value = ui->lineRemoteURL->text();
	settings->Mappings[FUEL_SETTING_IGNORE_GLOB].Value = ui->lineIgnore->text();
	settings->Mappings[FUEL_SETTING_CRNL_GLOB].Value = ui->lineIgnoreCRNL->text();
}
//-----------------------------------------------------------------------------
void SettingsDialog::on_btnSelectFossil_clicked()
{
	QString path = SelectExe(this, tr("Fossil executable"));
	if(!path.isEmpty())
		ui->lineFossilPath->setText(QDir::toNativeSeparators(path));
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_btnSelectFossilGDiff_clicked()
{
	QString path = SelectExe(this, tr("Graphical Diff application"));
	if(!path.isEmpty())
		ui->lineGDiffCommand->setText(QDir::toNativeSeparators(path));
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_btnSelectGMerge_clicked()
{
	QString path = SelectExe(this, tr("Graphical Merge application"));
	if(!path.isEmpty())
		ui->lineGMergeCommand->setText(QDir::toNativeSeparators(path));
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_btnClearMessageHistory_clicked()
{
	if(DialogQuery(this, tr("Clear Commit Message History"), tr("Are you sure want to clear the commit message history?"))==ANSWER_YES)
		settings->Mappings[FUEL_SETTING_COMMIT_MSG].Value = QStringList();
}
