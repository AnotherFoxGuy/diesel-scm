#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>
#include <QCompleter>

namespace Ui {
	class UpdateDialog;
}

class UpdateDialog : public QDialog
{
	Q_OBJECT

public:
	explicit UpdateDialog(QWidget *parent, const QStringList &completions, const QString &defaultValue);
	~UpdateDialog();

	static QString run(QWidget *parent, const QStringList &completions, const QString &defaultValue);


private:
	Ui::UpdateDialog *ui;
	QCompleter	completer;
};

#endif // UPDATEDIALOG_H
