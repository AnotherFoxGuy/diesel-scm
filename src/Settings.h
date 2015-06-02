#ifndef SETTINGS_H
#define SETTINGS_H

#include <QMap>
#include <QVariant>
#include <QTranslator>

#define FUEL_SETTING_FOSSIL_PATH			"FossilPath"
#define FUEL_SETTING_COMMIT_MSG				"CommitMsgHistory"
#define FUEL_SETTING_FILE_DBLCLICK			"FileDblClickAction"
#define FUEL_SETTING_LANGUAGE				"Language"
#define FUEL_SETTING_WEB_BROWSER			"WebBrowser"
#define FUEL_SETTING_FILEACTION_NAME		"FileActionName"
#define FUEL_SETTING_FILEACTION_COMMAND		"FileActionCommand"

#define FOSSIL_SETTING_GDIFF_CMD			"gdiff-command"
#define FOSSIL_SETTING_GMERGE_CMD			"gmerge-command"
#define FOSSIL_SETTING_PROXY_URL			"proxy"
#define FOSSIL_SETTING_IGNORE_GLOB			"ignore-glob"
#define FOSSIL_SETTING_CRNL_GLOB			"crnl-glob"
#define FOSSIL_SETTING_HTTP_PORT			"http-port"


enum FileDblClickAction
{
	FILE_DLBCLICK_ACTION_DIFF,
	FILE_DLBCLICK_ACTION_OPEN,
	FILE_DLBCLICK_ACTION_OPENCONTAINING,
	FILE_DLBCLICK_ACTION_MAX
};

struct Settings
{
	struct Setting
	{
		enum SettingType
		{
			TYPE_FOSSIL_GLOBAL,
			TYPE_FOSSIL_LOCAL
		};

		Setting(QVariant value, SettingType type) : Value(value), Type(type)
		{}
		QVariant Value;
		SettingType Type;
	};
	typedef QMap<QString, Setting> mappings_t;


	Settings(bool portableMode = false);
	~Settings();

	void				ApplyEnvironment();

	// App configuration access
	class QSettings *	GetStore() { return store; }
	bool				HasValue(const QString &name) const; // store->contains(FUEL_SETTING_FOSSIL_PATH)
	const QVariant		GetValue(const QString &name); // settings.store->value
	void				SetValue(const QString &name, const QVariant &value); // settings.store->value

	// Fossil configuration access
	QVariant &		GetFossilValue(const QString &name);
	void			SetFossilValue(const QString &name, const QVariant &value);
	mappings_t &	GetMappings() { return Mappings; }

	bool			SupportsLang(const QString &langId) const;
	bool			InstallLang(const QString &langId);
private:
	mappings_t		Mappings;
	class QSettings	*store;
	QTranslator		translator;
};


#endif // SETTINGS_H
