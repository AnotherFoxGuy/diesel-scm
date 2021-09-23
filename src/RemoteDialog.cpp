#include "RemoteDialog.h"
#include "Utils.h"
#include "ui_RemoteDialog.h"
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QUrl>

//-----------------------------------------------------------------------------
RemoteDialog::RemoteDialog(QWidget *parent) : QDialog(parent), ui(new Ui::RemoteDialog)
{
    ui->setupUi(this);
}

//-----------------------------------------------------------------------------
RemoteDialog::~RemoteDialog()
{
    delete ui;
}

//-----------------------------------------------------------------------------
bool RemoteDialog::run(QWidget *parent, QUrl &url, QString &name)
{
    RemoteDialog dlg(parent);

    // Set URL components
    if (!url.isEmpty())
    {
        dlg.ui->lineName->setText(name);

        if (url.isLocalFile())
            dlg.ui->lineURL->setText(QDir::toNativeSeparators(url.toLocalFile()));
        else
        {
            QString url_no_credentials = UrlToStringNoCredentials(url);
            dlg.ui->lineURL->setText(url_no_credentials);
            dlg.ui->lineUserName->setText(url.userName());
            dlg.ui->linePassword->setText(url.password());
        }
    }

    if (dlg.exec() != QDialog::Accepted)
        return false;

    QString urltext = dlg.ui->lineURL->text();

    // Check if the url is a local file
    if (QFileInfo(urltext).exists())
        url = QUrl::fromLocalFile(urltext);
    else
    {
        url = QUrl::fromUserInput(urltext);
        if (url.isEmpty() || !url.isValid())
        {
            QMessageBox::critical(parent, tr("Error"), tr("Invalid URL."), QMessageBox::Ok);
            return false;
        }
    }

    if (!dlg.ui->lineUserName->text().trimmed().isEmpty())
        url.setUserName(dlg.ui->lineUserName->text());

    if (!dlg.ui->linePassword->text().trimmed().isEmpty())
        url.setPassword(dlg.ui->linePassword->text());

    name = dlg.ui->lineName->text().trimmed();
    if (name.isEmpty())
        name = UrlToStringNoCredentials(url);

    return true;
}

//-----------------------------------------------------------------------------
void RemoteDialog::GetRepositoryPath(QString &pathResult)
{
    QString filter(tr("Fossil Repository") + QString(" (*." FOSSIL_EXT ")"));

    pathResult = QFileDialog::getSaveFileName(this, tr("Select Fossil Repository"), QDir::toNativeSeparators(pathResult), filter, &filter, QFileDialog::DontConfirmOverwrite);
}

//-----------------------------------------------------------------------------
void RemoteDialog::on_btnSelectSourceRepo_clicked()
{
    QString path = ui->lineURL->text();
    GetRepositoryPath(path);

    if (path.trimmed().isEmpty())
        return;

    if (!QFile::exists(path))
    {
        QMessageBox::critical(this, tr("Error"), tr("Invalid Repository File."), QMessageBox::Ok);
        return;
    }

    ui->lineURL->setText(QDir::toNativeSeparators(path));
}
