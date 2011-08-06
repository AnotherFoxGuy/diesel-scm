#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

namespace Ui {
    class SettingsDialog;
}

struct Settings
{
	QString				fossilPath;

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

private:
    Ui::SettingsDialog *ui;
	Settings *settings;
};

#endif // SETTINGSDIALOG_H
