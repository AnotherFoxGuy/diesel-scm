#ifndef FILEACTIONDIALOG_H
#define FILEACTIONDIALOG_H

#include <QDialog>
#include <QStandardItemModel>
#include <QCheckBox>

namespace Ui {
    class FileActionDialog;
}

class FileActionDialog : public QDialog
{
    Q_OBJECT

public:
	explicit FileActionDialog(QWidget *parent, const QString &title, const QString &message, const QStringList &files, const QString &checkBoxText=QString(), bool *checkBoxResult=0);
    ~FileActionDialog();

	static bool run(QWidget *parent, const QString &title, const QString &message, const QStringList &files, const QString &checkBoxText=QString(), bool *checkBoxResult=0);

private:
    Ui::FileActionDialog *ui;
	QStandardItemModel	itemModel;
	QCheckBox *checkBox;
	bool *checkBoxResult;
};

#endif // FILEACTIONDIALOG_H
