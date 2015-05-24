#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include "Settings.h"

namespace Ui {
	class SettingsDialog;
}

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
