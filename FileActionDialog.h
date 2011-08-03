#ifndef FILEACTIONDIALOG_H
#define FILEACTIONDIALOG_H

#include <QDialog>
#include <QStandardItemModel>

namespace Ui {
    class FileActionDialog;
}

class FileActionDialog : public QDialog
{
    Q_OBJECT

public:
	explicit FileActionDialog(const QString &title, const QString &message, const QStringList &files, QWidget *parent = 0);
    ~FileActionDialog();

	static bool run(const QString &title, const QString &message, const QStringList &files, QWidget *parent);

private:
    Ui::FileActionDialog *ui;
	QStandardItemModel	itemModel;
};

#endif // FILEACTIONDIALOG_H
