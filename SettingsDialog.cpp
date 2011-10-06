#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include <QFileDialog>

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
	ui->lineFossilPath->setText(settings->fossilPath);
	ui->lineGDiffCommand->setText(settings->gDiffCommand);
	ui->lineGMergeCommand->setText(settings->gMergeCommand);
	ui->lineIgnore->setText(settings->ignoreGlob);
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
	settings->fossilPath = ui->lineFossilPath->text();
	settings->gDiffCommand = ui->lineGDiffCommand->text();
	settings->gMergeCommand = ui->lineGMergeCommand->text();
	settings->ignoreGlob = ui->lineIgnore->text();
}
//-----------------------------------------------------------------------------
void SettingsDialog::on_btnSelectFossil_clicked()
{
	QString path = SelectExe(this, tr("Fossil executable"));
	if(!path.isEmpty())
		ui->lineFossilPath->setText(path);
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_btnSelectFossilGDiff_clicked()
{
	QString path = SelectExe(this, tr("Graphical Diff application"));
	if(!path.isEmpty())
		ui->lineGDiffCommand->setText(path);
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_btnSelectGMerge_clicked()
{
	QString path = SelectExe(this, tr("Graphical Merge application"));
	if(!path.isEmpty())
		ui->lineGMergeCommand->setText(path);
}
