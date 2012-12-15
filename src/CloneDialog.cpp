#include "CloneDialog.h"
#include "ui_CloneDialog.h"
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QClipboard>
#include <QUrl>

//-----------------------------------------------------------------------------
CloneDialog::CloneDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::CloneDialog)
{
	ui->setupUi(this);
}

//-----------------------------------------------------------------------------
CloneDialog::~CloneDialog()
{
	delete ui;
}

//-----------------------------------------------------------------------------
bool CloneDialog::run(QWidget *parent, QUrl &url, QString &repository)
{
	CloneDialog dlg(parent);

	// Try to parse a url from the clipboard
	QClipboard *clipboard = QApplication::clipboard();
	if(clipboard)
	{
		QUrl nurl = QUrl::fromUserInput(clipboard->text());

		// If we have a valid url
		if(nurl.isValid() && !nurl.isEmpty())
		{
			// Fill in dialog
			dlg.ui->lineUserName->setText(nurl.userName());
			dlg.ui->linePassword->setText(nurl.password());
			nurl.setUserName("");
			nurl.setPassword("");
			dlg.ui->lineURL->setText(nurl.toString());
		}
	}

	if(dlg.exec() != QDialog::Accepted)
		return false;

	url.setUrl(dlg.ui->lineURL->text());
	if(url.isEmpty() || !url.isValid())
	{
		QMessageBox::critical(parent, tr("Error"), tr("Invalid URL."), QMessageBox::Ok );
		return false;
	}

	url.setUserName(dlg.ui->lineUserName->text());
	url.setPassword(dlg.ui->linePassword->text());

	if(dlg.ui->lineRepository->text().isEmpty() )
	{
		QMessageBox::critical(parent, tr("Error"), tr("Invalid Repository File."), QMessageBox::Ok );
		return false;
	}

	repository = dlg.ui->lineRepository->text();
	return true;
}

//-----------------------------------------------------------------------------
void CloneDialog::on_btnSelectRepository_clicked()
{
	QString filter(tr("Fossil Repository") + QString(" (*." FOSSIL_EXT ")"));

	QString path = QFileDialog::getSaveFileName(
				this,
				tr("Select Fossil Repository"),
				QDir::toNativeSeparators(ui->lineRepository->text()),
				filter,
				&filter,
				QFileDialog::DontConfirmOverwrite);

	if(path.isEmpty())
		return;

	if(QFile::exists(path))
	{
		QMessageBox::critical(this, tr("Error"), tr("This repository file already exists."), QMessageBox::Ok );
		return;
	}

	ui->lineRepository->setText(QDir::toNativeSeparators(path));
}
