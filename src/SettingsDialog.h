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
#define FUEL_SETTING_GDIFF_CMD				"gdiff-command"
#define FUEL_SETTING_GMERGE_CMD				"gmerge-command"
#define FUEL_SETTING_IGNORE_GLOB			"ignore-glob"
#define FUEL_SETTING_CRNL_GLOB				"crnl-glob"
#define FUEL_SETTING_REMOTE_URL				"remote-url"

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
			TYPE_INTERNAL,
			TYPE_FOSSIL_GLOBAL,
			TYPE_FOSSIL_LOCAL,
			TYPE_FOSSIL_COMMAND
		};

		Setting(QVariant value=QVariant(), SettingType type=TYPE_INTERNAL) : Value(value), Type(type)
		{}
		QVariant Value;
		SettingType Type;
	};

	Settings(bool portableMode = false);
	~Settings();
	void ApplyEnvironment();

	typedef QMap<QString, Setting> mappings_t;
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

	Ui::SettingsDialog *ui;
	Settings *settings;
};

#endif // SETTINGSDIALOG_H
