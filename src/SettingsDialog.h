#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QMap>
#include <QVariant>
#include <QTranslator>


namespace Ui {
	class SettingsDialog;
}

#define FUEL_SETTING_FOSSIL_PATH			"FossilPath"
#define FUEL_SETTING_COMMIT_MSG				"CommitMsgHistory"
#define FUEL_SETTING_FILE_DBLCLICK			"FileDblClickAction"
#define FUEL_SETTING_LANGUAGE				"Language"
#define FUEL_SETTING_WEB_BROWSER			"WebBrowser"
#define FUEL_SETTING_HTTP_PORT				"HTTPPort"

#define FOSSIL_SETTING_GDIFF_CMD			"gdiff-command"
#define FOSSIL_SETTING_GMERGE_CMD			"gmerge-command"
#define FOSSIL_SETTING_PROXY_URL			"proxy"
#define FOSSIL_SETTING_IGNORE_GLOB			"ignore-glob"
#define FOSSIL_SETTING_CRNL_GLOB			"crnl-glob"
#define FOSSIL_SETTING_REMOTE_URL			"remote-url"


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
			TYPE_FOSSIL_LOCAL,
			TYPE_FOSSIL_COMMAND
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


class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SettingsDialog(QWidget *parent, Settings &_settings);
	~SettingsDialog();

	static bool run(QWidget *parent, Settings &_settings);


private slots:
	void on_btnSelectFossil_clicked();
	void on_buttonBox_accepted();
	void on_btnSelectFossilGDiff_clicked();
	void on_btnSelectGMerge_clicked();
	void on_btnClearMessageHistory_clicked();

private:
	static QString SelectExe(QWidget *parent, const QString &description);
	QString LangIdToName(const QString &id);
	QString LangNameToId(const QString &name);
	void CreateLangMap();

	struct LangMap
	{
		LangMap(const QString &_id, const QString &_name)
			: id(_id), name(_name)
		{
		}

		QString id;
		QString name;
	};

	QList<LangMap>	langMap;
	Ui::SettingsDialog *ui;
	Settings *settings;
};

#endif // SETTINGSDIALOG_H
