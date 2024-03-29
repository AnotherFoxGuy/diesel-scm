#include "RevisionDialog.h"
#include "Utils.h"
#include "ui_RevisionDialog.h"

//-----------------------------------------------------------------------------
RevisionDialog::RevisionDialog(QWidget *parent, const QStringList &completions, const QString &defaultValue) : QDialog(parent), ui(new Ui::RevisionDialog), completer(completions, parent)
{
    ui->setupUi(this);
    ui->cmbRevision->setCompleter(&completer);

    ui->cmbRevision->addItems(completions);

    if (!defaultValue.isEmpty())
    {
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        int index = ui->cmbRevision->findText(defaultValue);
        if (index >= 0)
            ui->cmbRevision->setCurrentIndex(index);
#else
        ui->cmbRevision->setCurrentText(defaultValue);
#endif
    }
}

//-----------------------------------------------------------------------------
RevisionDialog::~RevisionDialog()
{
    delete ui;
}

//-----------------------------------------------------------------------------
QString RevisionDialog::runUpdate(QWidget *parent, const QString &title, const QStringList &completions, const QString &defaultValue)
{
    RevisionDialog dlg(parent, completions, defaultValue);
    dlg.setWindowTitle(title);
    dlg.ui->lblName->setVisible(false);
    dlg.ui->lineName->setVisible(false);
    dlg.ui->lblIntegrate->setVisible(false);
    dlg.ui->chkIntegrate->setVisible(false);
    dlg.ui->lblForce->setVisible(false);
    dlg.ui->chkForce->setVisible(false);

    dlg.adjustSize();

    if (dlg.exec() != QDialog::Accepted)
        return QString("");
    return dlg.ui->cmbRevision->currentText().trimmed();
}

//-----------------------------------------------------------------------------
QString RevisionDialog::runMerge(QWidget *parent, const QString &title, const QStringList &completions, const QString &defaultValue, bool &integrate, bool &force)
{
    RevisionDialog dlg(parent, completions, defaultValue);
    dlg.setWindowTitle(title);
    dlg.ui->lblName->setVisible(false);
    dlg.ui->lineName->setVisible(false);
    dlg.ui->lblIntegrate->setVisible(true);
    dlg.ui->chkIntegrate->setVisible(true);
    dlg.ui->chkIntegrate->setChecked(integrate);
    dlg.ui->lblForce->setVisible(true);
    dlg.ui->chkForce->setVisible(true);
    dlg.ui->chkForce->setChecked(force);

    dlg.adjustSize();

    if (dlg.exec() != QDialog::Accepted)
        return QString("");

    integrate = dlg.ui->chkIntegrate->checkState() == Qt::Checked;
    force = dlg.ui->chkForce->checkState() == Qt::Checked;

    return dlg.ui->cmbRevision->currentText().trimmed();
}

//-----------------------------------------------------------------------------
bool RevisionDialog::runNewTag(QWidget *parent, const QString &title, const QStringList &completions, const QString &defaultValue, QString &revision, QString &name)
{
    RevisionDialog dlg(parent, completions, defaultValue);
    dlg.setWindowTitle(title);

    dlg.ui->lblName->setVisible(true);
    dlg.ui->lineName->setVisible(true);
    dlg.ui->lblIntegrate->setVisible(false);
    dlg.ui->chkIntegrate->setVisible(false);
    dlg.ui->lblForce->setVisible(false);
    dlg.ui->chkForce->setVisible(false);

    dlg.adjustSize();

    if (dlg.exec() != QDialog::Accepted)
        return false;

    revision = dlg.ui->cmbRevision->currentText().trimmed();
    name = dlg.ui->lineName->text().trimmed();
    return true;
}
