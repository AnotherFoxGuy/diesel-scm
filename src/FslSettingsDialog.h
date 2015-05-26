#ifndef FSLSETTINGSDIALOG_H
#define FSLSETTINGSDIALOG_H

#include <QDialog>
#include "Settings.h"

namespace Ui {
	class FslSettingsDialog;
}

class FslSettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit FslSettingsDialog(QWidget *parent, Settings &_settings);
	~FslSettingsDialog();

	static bool run(QWidget *parent, Settings &_settings);


private slots:
	void on_buttonBox_accepted();
	void on_btnSelectFossilGDiff_clicked();
	void on_btnSelectGMerge_clicked();

private:

	Ui::FslSettingsDialog *ui;
	Settings *settings;
};

#endif // FSLSETTINGSDIALOG_H
