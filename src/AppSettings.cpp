#include "AppSettings.h"

#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QTranslator>
#include <QResource>
#include <QTextCodec>

///////////////////////////////////////////////////////////////////////////////
Settings::Settings(bool portableMode) : store(0)
{
	Mappings.insert(FOSSIL_SETTING_GDIFF_CMD, Setting("", Setting::TYPE_FOSSIL_GLOBAL));
	Mappings.insert(FOSSIL_SETTING_GMERGE_CMD, Setting("", Setting::TYPE_FOSSIL_GLOBAL));
	Mappings.insert(FOSSIL_SETTING_PROXY_URL, Setting("", Setting::TYPE_FOSSIL_GLOBAL));
	Mappings.insert(FOSSIL_SETTING_HTTP_PORT, Setting("", Setting::TYPE_FOSSIL_GLOBAL));

	Mappings.insert(FOSSIL_SETTING_IGNORE_GLOB, Setting("", Setting::TYPE_FOSSIL_LOCAL));
	Mappings.insert(FOSSIL_SETTING_CRNL_GLOB, Setting("", Setting::TYPE_FOSSIL_LOCAL));

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


	for(int i=0; i<MAX_CUSTOM_ACTIONS; ++i)
	{
		CustomAction action;
		action.Id = QObject::tr("Custom Action %0").arg(i+1);
		customActions.append(action);
	}

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
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
	QTextCodec::setCodecForTr(QTextCodec::codecForName("utf8"));
#endif
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

	QString locale_path = QString(":intl/%0.qm").arg(langId);
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
	if(!HasValue(name))
		return QVariant();
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
