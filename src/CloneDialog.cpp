#include "CloneDialog.h"
#include "Utils.h"
#include "ui_CloneDialog.h"
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QUrl>

//-----------------------------------------------------------------------------
CloneDialog::CloneDialog(QWidget *parent) : QDialog(parent), ui(new Ui::CloneDialog)
{
    ui->setupUi(this);
}

//-----------------------------------------------------------------------------
CloneDialog::~CloneDialog()
{
    delete ui;
}

//-----------------------------------------------------------------------------
bool CloneDialog::run(QWidget *parent, QUrl &url, QString &repository, QUrl &urlProxy)
{
    CloneDialog dlg(parent);

    // Try to parse a url from the clipboard
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard)
    {
        QUrl nurl = QUrl::fromUserInput(clipboard->text());

        // If we have a valid url
        if (nurl.isValid() && !nurl.isEmpty())
        {
            // Fill in dialog
            dlg.ui->lineUserName->setText(nurl.userName());
            dlg.ui->linePassword->setText(nurl.password());
            nurl.setUserName("");
            nurl.setPassword("");
            dlg.ui->lineURL->setText(UrlToStringNoCredentials(nurl));
        }
    }

    if (dlg.exec() != QDialog::Accepted)
        return false;

    QString urltext = dlg.ui->lineURL->text();

    // Check if the url is a local file
    if (QFileInfo::exists(urltext))
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

    if (dlg.ui->lineTargetRepository->text().isEmpty())
    {
        QMessageBox::critical(parent, tr("Error"), tr("Invalid Repository File."), QMessageBox::Ok);
        return false;
    }

    urlProxy = QUrl::fromUserInput(dlg.ui->lineHttpProxyUrl->text());
    if (!urlProxy.isEmpty() && !urlProxy.isValid())
    {
        QMessageBox::critical(parent, tr("Error"), tr("Invalid Proxy URL."), QMessageBox::Ok);
        return false;
    }

    repository = dlg.ui->lineTargetRepository->text();
    return true;
}

//-----------------------------------------------------------------------------
void CloneDialog::GetRepositoryPath(QString &pathResult)
{
    QString filter(tr("Fossil Repository") + QString(" (*." FOSSIL_EXT ")"));

    pathResult = QFileDialog::getSaveFileName(this, tr("Select Fossil Repository"), QDir::toNativeSeparators(pathResult), filter, &filter, QFileDialog::DontConfirmOverwrite);

    // Ensure that it ends in the required extension (On GTK, Qt doesn't seem to add it automatically)
    QFileInfo fi(pathResult);
    QString ext = fi.suffix().toLower();
    if (ext != FOSSIL_EXT)
        pathResult += "." FOSSIL_EXT;
}

//-----------------------------------------------------------------------------
void CloneDialog::on_btnSelectSourceRepo_clicked()
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

//-----------------------------------------------------------------------------
void CloneDialog::on_btnSelectTargetRepo_clicked()
{
    QString path = ui->lineTargetRepository->text();
    GetRepositoryPath(path);

    if (path.trimmed().isEmpty())
        return;

    if (QFile::exists(path))
    {
        QMessageBox::critical(this, tr("Error"), tr("This repository file already exists."), QMessageBox::Ok);
        return;
    }

    ui->lineTargetRepository->setText(QDir::toNativeSeparators(path));
}
