#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget *parent, Settings &_settings) :
	QDialog(parent, Qt::Sheet),
	ui(new Ui::SettingsDialog),
	settings(&_settings)
{
	ui->setupUi(this);
	ui->lineEdit->setText(settings->fossilPath);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::on_btnSelectFossil_clicked()
{
#ifdef Q_WS_WIN
	QString filter(tr("Fossil Executables (*.exe)"));
#else
	QString filter(tr("Fossil Executables (*)"));
#endif
	QString path = QFileDialog::getOpenFileName(
				this,
				tr("Select Fossil Executable"),
				QString(),
				filter,
				&filter);

	if(QFile::exists(path))
	{
		ui->lineEdit->setText(path)	;
	}
}


bool SettingsDialog::run(QWidget *parent, Settings &settings)
{
	SettingsDialog dlg(parent, settings);
	return dlg.exec() == QDialog::Accepted;
}

void SettingsDialog::on_buttonBox_accepted()
{
	settings->fossilPath = ui->lineEdit->text();
}
