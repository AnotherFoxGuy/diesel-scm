#ifndef REPODIALOG_H
#define REPODIALOG_H

#include <QDialog>

namespace Ui {
    class RepoDialog;
}

class RepoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RepoDialog(QWidget *parent = 0);
    ~RepoDialog();

private:
    Ui::RepoDialog *ui;
};

#endif // REPODIALOG_H
