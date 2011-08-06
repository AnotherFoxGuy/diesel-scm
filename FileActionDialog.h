#ifndef FILEACTIONDIALOG_H
#define FILEACTIONDIALOG_H

#include <QDialog>
#include <QStandardItemModel>
#include <QDialogButtonBox>

namespace Ui {
    class FileActionDialog;
}

class FileActionDialog : public QDialog
{
    Q_OBJECT
public:
	explicit FileActionDialog(QWidget *parent, const QString &title, const QString &message, const QStringList &listData, const QString &checkBoxText=QString(), bool *checkBoxResult=0);
    ~FileActionDialog();

	static bool run(QWidget *parent, const QString &title, const QString &message, const QStringList &listData, const QString &checkBoxText=QString(), bool *checkBoxResult=0);

	typedef QDialogButtonBox::StandardButton StandardButton;
	typedef QDialogButtonBox::StandardButtons StandardButtons;

	static StandardButton runStandardButtons(QWidget *parent, StandardButtons, const QString &title, const QString &message, const QStringList &listData);


private slots:
	void on_buttonBox_clicked(QAbstractButton *button);

private:
    Ui::FileActionDialog *ui;
	QStandardItemModel	itemModel;
	class QCheckBox *checkBox;
	bool *checkBoxResult;
	StandardButton clickedButton;
};

#endif // FILEACTIONDIALOG_H
