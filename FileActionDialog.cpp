#include "FileActionDialog.h"
#include "ui_FileActionDialog.h"

FileActionDialog::FileActionDialog(const QString &title, const QString &message, const QStringList &files, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FileActionDialog)
{
    ui->setupUi(this);
	setWindowTitle(title);
	ui->label->setText(message);
	ui->listView->setModel(&itemModel);

	for(QStringList::const_iterator it=files.begin(); it!=files.end(); ++it)
		itemModel.appendRow(new QStandardItem(*it));
}

FileActionDialog::~FileActionDialog()
{
	delete ui;
}

bool FileActionDialog::run(const QString &title, const QString &message, const QStringList &files, QWidget *parent)
{
	FileActionDialog dlg(title, message, files, parent);
	int res = dlg.exec();

	return res == QDialog::Accepted;
}
