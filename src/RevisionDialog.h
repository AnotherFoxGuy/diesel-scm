#ifndef REVISIONDIALOG_H
#define REVISIONDIALOG_H

#include <QCompleter>
#include <QDialog>

namespace Ui
{
class RevisionDialog;
}

class RevisionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RevisionDialog(QWidget *parent, const QStringList &completions, const QString &defaultValue);
    ~RevisionDialog();

    static QString runUpdate(QWidget *parent, const QString &title, const QStringList &completions, const QString &defaultValue);
    static QString runMerge(QWidget *parent, const QString &title, const QStringList &completions, const QString &defaultValue, bool &integrate, bool &force);
    static bool runNewTag(QWidget *parent, const QString &title, const QStringList &completions, const QString &defaultValue, QString &revision, QString &name);

private:
    Ui::RevisionDialog *ui;
    QCompleter completer;
};

#endif  // REVISIONDIALOG_H
