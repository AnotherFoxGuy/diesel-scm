#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QMap>

namespace Ui {
    class SettingsDialog;
}

struct Settings
{
	QString				fossilPath;
	QString				gDiffCommand;
	QString				gMergeCommand;
	QString				ignoreGlob;

	typedef QMap<QString, QString *> mappings_t;
	mappings_t	Mappings;

	Settings()
	{
		Mappings.insert("gdiff-command", &gDiffCommand);
		Mappings.insert("gmerge-command", &gMergeCommand);
		Mappings.insert("ignore-glob", &ignoreGlob);
	}
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

private:
    Ui::SettingsDialog *ui;
	Settings *settings;
};

#endif // SETTINGSDIALOG_H
