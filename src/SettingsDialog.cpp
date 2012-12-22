#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include <QFileDialog>
#include "Utils.h"

#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QTranslator>
#include <QResource>
#include <QTextCodec>


///////////////////////////////////////////////////////////////////////////////
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
				description,
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

	CreateLangMap();

	ui->cmbDoubleClickAction->addItem(tr("Diff File"));
	ui->cmbDoubleClickAction->addItem(tr("Open File"));
	ui->cmbDoubleClickAction->addItem(tr("Open Containing Folder"));

	ui->cmbFossilBrowser->addItem(tr("System"));
	ui->cmbFossilBrowser->addItem(tr("Internal"));

	// App Settings
	ui->lineFossilPath->setText(QDir::toNativeSeparators(settings->GetValue(FUEL_SETTING_FOSSIL_PATH).toString()));
	ui->cmbDoubleClickAction->setCurrentIndex(settings->GetValue(FUEL_SETTING_FILE_DBLCLICK).toInt());
	ui->cmbFossilBrowser->setCurrentIndex(settings->GetValue(FUEL_SETTING_WEB_BROWSER).toInt());
	ui->lineUIPort->setText(settings->GetValue(FUEL_SETTING_HTTP_PORT).toString());

	// Initialize language combo
	foreach(const LangMap &m, langMap)
		ui->cmbActiveLanguage->addItem(m.name);

	QString lang = settings->GetValue(FUEL_SETTING_LANGUAGE).toString();
	// Select current language
	ui->cmbActiveLanguage->setCurrentIndex(
				ui->cmbActiveLanguage->findText(
					LangIdToName(lang)));

	// Repo Settings
	ui->lineGDiffCommand->setText(QDir::toNativeSeparators(settings->GetFossilValue(FOSSIL_SETTING_GDIFF_CMD).toString()));
	ui->lineGMergeCommand->setText(QDir::toNativeSeparators(settings->GetFossilValue(FOSSIL_SETTING_GMERGE_CMD).toString()));
	ui->lineRemoteURL->setText(settings->GetFossilValue(FOSSIL_SETTING_REMOTE_URL).toString());
	ui->lineIgnore->setText(settings->GetFossilValue(FOSSIL_SETTING_IGNORE_GLOB).toString());
	ui->lineIgnoreCRNL->setText(settings->GetFossilValue(FOSSIL_SETTING_CRNL_GLOB).toString());
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
	settings->SetValue(FUEL_SETTING_FOSSIL_PATH, QDir::fromNativeSeparators(ui->lineFossilPath->text()));
	Q_ASSERT(ui->cmbDoubleClickAction->currentIndex()>=FILE_DLBCLICK_ACTION_DIFF && ui->cmbDoubleClickAction->currentIndex()<FILE_DLBCLICK_ACTION_MAX);
	settings->SetValue(FUEL_SETTING_FILE_DBLCLICK, ui->cmbDoubleClickAction->currentIndex());
	settings->SetValue(FUEL_SETTING_WEB_BROWSER, ui->cmbFossilBrowser->currentIndex());
	settings->SetValue(FUEL_SETTING_HTTP_PORT, ui->lineUIPort->text());

	Q_ASSERT(settings->HasValue(FUEL_SETTING_LANGUAGE));
	QString curr_langid = settings->GetValue(FUEL_SETTING_LANGUAGE).toString();
	QString new_langid = LangNameToId(ui->cmbActiveLanguage->currentText());
	Q_ASSERT(!new_langid.isEmpty());
	settings->SetValue(FUEL_SETTING_LANGUAGE, new_langid);

	if(curr_langid != new_langid)
		QMessageBox::information(this, tr("Restart required"), tr("The language change will take effect after restarting the application"), QMessageBox::Ok);

	settings->SetFossilValue(FOSSIL_SETTING_GDIFF_CMD, QDir::fromNativeSeparators(ui->lineGDiffCommand->text()));
	settings->SetFossilValue(FOSSIL_SETTING_GMERGE_CMD, QDir::fromNativeSeparators(ui->lineGMergeCommand->text()));
	settings->SetFossilValue(FOSSIL_SETTING_REMOTE_URL, ui->lineRemoteURL->text());
	settings->SetFossilValue(FOSSIL_SETTING_IGNORE_GLOB, ui->lineIgnore->text());
	settings->SetFossilValue(FOSSIL_SETTING_CRNL_GLOB, ui->lineIgnoreCRNL->text());

	settings->ApplyEnvironment();
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_btnSelectFossil_clicked()
{
	QString path = SelectExe(this, tr("Select Fossil executable"));
	if(!path.isEmpty())
		ui->lineFossilPath->setText(QDir::toNativeSeparators(path));
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_btnSelectFossilGDiff_clicked()
{
	QString path = SelectExe(this, tr("Select Graphical Diff application"));
	if(!path.isEmpty())
		ui->lineGDiffCommand->setText(QDir::toNativeSeparators(path));
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_btnSelectGMerge_clicked()
{
	QString path = SelectExe(this, tr("Select Graphical Merge application"));
	if(!path.isEmpty())
		ui->lineGMergeCommand->setText(QDir::toNativeSeparators(path));
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_btnClearMessageHistory_clicked()
{
	if(DialogQuery(this, tr("Clear Commit Message History"), tr("Are you sure you want to clear the commit message history?"))==QMessageBox::Yes)
		settings->SetValue(FUEL_SETTING_COMMIT_MSG, QStringList());
}

//-----------------------------------------------------------------------------
void SettingsDialog::CreateLangMap()
{
	langMap.append(LangMap("de_DE", "German (DE)"));
	langMap.append(LangMap("el_GR", "Greek"));
	langMap.append(LangMap("en_US", "English (US)"));
	langMap.append(LangMap("es_ES", "Spanish (ES)"));
	langMap.append(LangMap("fr_FR", "French (FR)"));
}

//-----------------------------------------------------------------------------
QString SettingsDialog::LangIdToName(const QString &id)
{
	foreach(const LangMap &m, langMap)
	{
		if(m.id == id)
			return m.name;
	}

	return "";
}

//-----------------------------------------------------------------------------
QString SettingsDialog::LangNameToId(const QString &name)
{
	foreach(const LangMap &m, langMap)
	{
		if(m.name == name)
			return m.id;
	}

	return "";
}

///////////////////////////////////////////////////////////////////////////////
Settings::Settings(bool portableMode) : store(0)
{
	Mappings.insert(FOSSIL_SETTING_GDIFF_CMD, Setting("", Setting::TYPE_FOSSIL_GLOBAL));
	Mappings.insert(FOSSIL_SETTING_GMERGE_CMD, Setting("", Setting::TYPE_FOSSIL_GLOBAL));
	Mappings.insert(FOSSIL_SETTING_IGNORE_GLOB, Setting("", Setting::TYPE_FOSSIL_LOCAL));
	Mappings.insert(FOSSIL_SETTING_CRNL_GLOB, Setting("", Setting::TYPE_FOSSIL_LOCAL));
	Mappings.insert(FOSSIL_SETTING_REMOTE_URL, Setting("off", Setting::TYPE_FOSSIL_COMMAND));

	// Go into portable mode when explicitly requested or if a config file exists next to the executable
	QString ini_path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + QDir::separator() + QCoreApplication::applicationName() + ".ini");
	if( portableMode || QFile::exists(ini_path))
		store = new QSettings(ini_path, QSettings::IniFormat);
	else
	{
		// Linux: ~/.config/organizationName/applicationName.conf
		// Windows: HKEY_CURRENT_USER\Software\organizationName\Fuel
		store = new QSettings(QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
	}
	Q_ASSERT(store);

	if(!HasValue(FUEL_SETTING_FILE_DBLCLICK))
		SetValue(FUEL_SETTING_FILE_DBLCLICK, 0);
	if(!HasValue(FUEL_SETTING_LANGUAGE) && SupportsLang(QLocale::system().name()))
		SetValue(FUEL_SETTING_LANGUAGE, QLocale::system().name());
	if(!HasValue(FUEL_SETTING_WEB_BROWSER))
		SetValue(FUEL_SETTING_WEB_BROWSER, 0);
	if(!HasValue(FUEL_SETTING_HTTP_PORT))
		SetValue(FUEL_SETTING_HTTP_PORT, "8090");

	ApplyEnvironment();
}

//-----------------------------------------------------------------------------
Settings::~Settings()
{
	Q_ASSERT(store);
	delete store;
}

//-----------------------------------------------------------------------------
void Settings::ApplyEnvironment()
{
	QString lang_id = GetValue(FUEL_SETTING_LANGUAGE).toString();
	QTextCodec::setCodecForTr(QTextCodec::codecForName("utf8"));
	if(!InstallLang(lang_id))
		SetValue(FUEL_SETTING_LANGUAGE, "en_US");
}

//-----------------------------------------------------------------------------
bool Settings::InstallLang(const QString &langId)
{
	if(langId == "en_US")
	{
		QCoreApplication::instance()->removeTranslator(&translator);
		return true;
	}

	QString locale_path = QString(":intl/intl/%0.qm").arg(langId);
	if(!translator.load(locale_path))
		return false;

	Q_ASSERT(!translator.isEmpty());
	QCoreApplication::instance()->installTranslator(&translator);


	return true;
}

//-----------------------------------------------------------------------------
bool Settings::HasValue(const QString &name) const
{
	return store->contains(name);
}

//-----------------------------------------------------------------------------
const QVariant Settings::GetValue(const QString &name)
{
	Q_ASSERT(HasValue(name));
	return store->value(name);
}

//-----------------------------------------------------------------------------
void Settings::SetValue(const QString &name, const QVariant &value)
{
	store->setValue(name, value);
}

//-----------------------------------------------------------------------------
QVariant &Settings::GetFossilValue(const QString &name)
{
	mappings_t::iterator it=Mappings.find(name);
	Q_ASSERT(it!=Mappings.end());
	return it.value().Value;
}

//-----------------------------------------------------------------------------
void Settings::SetFossilValue(const QString &name, const QVariant &value)
{
	mappings_t::iterator it=Mappings.find(name);
	Q_ASSERT(it!=Mappings.end());
	it->Value = value;
}

//-----------------------------------------------------------------------------
bool Settings::SupportsLang(const QString &langId) const
{
	QString locale_path = QString(":intl/intl/%0.qm").arg(langId);
	QResource res(locale_path);
	return res.isValid();
}
