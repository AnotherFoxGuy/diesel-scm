#ifndef CLONEDIALOG_H
#define CLONEDIALOG_H

#include <QDialog>

namespace Ui
{
class CloneDialog;
}

class CloneDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CloneDialog(QWidget *parent = 0);
    ~CloneDialog();

    static bool run(QWidget *parent, class QUrl &url, QString &repository, QUrl &urlProxy);

private slots:
    void on_btnSelectSourceRepo_clicked();
    void on_btnSelectTargetRepo_clicked();

private:
    void GetRepositoryPath(QString &pathResult);

    Ui::CloneDialog *ui;
};

#endif  // CLONEDIALOG_H
