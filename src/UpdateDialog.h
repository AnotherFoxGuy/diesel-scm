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

	static QString runUpdate(QWidget *parent, const QString &title, const QStringList &completions, const QString &defaultValue);
	static QString runMerge(QWidget* parent, const QString& title, const QStringList& completions, const QString& defaultValue, bool& integrate, bool& force);
	static bool runNewTag(QWidget *parent, const QString &title, const QStringList &completions, const QString &defaultValue, QString &revision, QString &name);

private:
	Ui::UpdateDialog *ui;
	QCompleter	completer;
};

#endif // UPDATEDIALOG_H
