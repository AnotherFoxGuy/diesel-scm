#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include <QFileDialog>
#include "Utils.h"

#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QTranslator>
#include <QResource>


QString SettingsDialog::SelectExe(QWidget *parent, const QString &description)
{
	QString filter(tr("Applications"));
#ifdef Q_WS_WIN
	filter += " (*.exe)";
#else
	filter += " (*)";
#endif
	QString path = QFileDialog::getOpenFileName(
				parent,
				tr("Select %1").arg(description),
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

	ui->cmbDoubleClickAction->addItem(tr("Diff File"));
	ui->cmbDoubleClickAction->addItem(tr("Open File"));
	ui->cmbDoubleClickAction->addItem(tr("Open Containing Folder"));

	// App Settings
	ui->lineFossilPath->setText(QDir::toNativeSeparators(settings->Mappings[FUEL_SETTING_FOSSIL_PATH].Value.toString()));
	ui->lineGDiffCommand->setText(QDir::toNativeSeparators(settings->Mappings[FUEL_SETTING_GDIFF_CMD].Value.toString()));
	ui->lineGMergeCommand->setText(QDir::toNativeSeparators(settings->Mappings[FUEL_SETTING_GMERGE_CMD].Value.toString()));
	ui->cmbDoubleClickAction->setCurrentIndex(settings->Mappings[FUEL_SETTING_FILE_DBLCLICK].Value.toInt());

	// Repo Settings
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
	Q_ASSERT(ui->cmbDoubleClickAction->currentIndex()>=FILE_DLBCLICK_ACTION_DIFF && ui->cmbDoubleClickAction->currentIndex()<FILE_DLBCLICK_ACTION_MAX);
	settings->Mappings[FUEL_SETTING_FILE_DBLCLICK].Value = ui->cmbDoubleClickAction->currentIndex();

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
	if(DialogQuery(this, tr("Clear Commit Message History"), tr("Are you sure want to clear the commit message history?"))==QMessageBox::Yes)
		settings->Mappings[FUEL_SETTING_COMMIT_MSG].Value = QStringList();
}

///////////////////////////////////////////////////////////////////////////////
Settings::Settings(bool portableMode) : store(0)
{
	Mappings[FUEL_SETTING_FOSSIL_PATH] = Setting();
	Mappings[FUEL_SETTING_COMMIT_MSG] = Setting();
	Mappings[FUEL_SETTING_FILE_DBLCLICK] = Setting(0); // Maps to FileDblClickAction
	Mappings[FUEL_SETTING_GDIFF_CMD] = Setting("", Setting::TYPE_FOSSIL_GLOBAL);
	Mappings[FUEL_SETTING_GMERGE_CMD] = Setting("", Setting::TYPE_FOSSIL_GLOBAL);
	Mappings[FUEL_SETTING_IGNORE_GLOB] = Setting("", Setting::TYPE_FOSSIL_LOCAL);
	Mappings[FUEL_SETTING_CRNL_GLOB] = Setting("", Setting::TYPE_FOSSIL_LOCAL);
	Mappings[FUEL_SETTING_REMOTE_URL] = Setting("off", Setting::TYPE_FOSSIL_COMMAND);


	// Go into portable mode when explicitly requested or if a config file exists next to the executable
	QString ini_path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + QDir::separator() + QCoreApplication::applicationName() + ".ini");
	if(portableMode || QFile::exists(ini_path))
		store = new QSettings(ini_path, QSettings::IniFormat);
	else
	{
		// Linux: ~/.config/organizationName/applicationName.conf
		// Windows: HKEY_CURRENT_USER\Software\organizationName\Fuel
		store = new QSettings(QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
	}

	ApplyEnvironment();
}

//-----------------------------------------------------------------------------
Settings::~Settings()
{
	delete store;
}

//-----------------------------------------------------------------------------
void Settings::ApplyEnvironment()
{
	QString locale_name = QLocale::system().name();
	//locale_name = "el_GR";

	QString locale_path = QString(":intl/intl/%0.qm").arg(locale_name);
	QResource res(locale_path);
	if(res.isValid() && translator.load(res.data(), res.size()))
		QCoreApplication::instance()->installTranslator(&translator);
}
