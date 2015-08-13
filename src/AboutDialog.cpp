#include "AboutDialog.h"
#include "ui_AboutDialog.h"
#include <QFile>

AboutDialog::AboutDialog(QWidget *parent, const QString &fossilVersion) :
	QDialog(parent),
	ui(new Ui::AboutDialog)
{
	ui->setupUi(this);
	ui->lblApp->setText(QCoreApplication::applicationName() + " "+ QCoreApplication::applicationVersion());
	ui->lblQtVersion->setText(tr("QT version %0").arg(QT_VERSION_STR));

	if(!fossilVersion.isEmpty())
		ui->lblFossilVersion->setText(tr("Fossil version %0").arg(fossilVersion));

	QFile ftrans(":/docs/docs/Translators.txt");
	if(ftrans.open(QFile::ReadOnly))
	{
		ui->txtTranslators->setText(ftrans.readAll());
		ftrans.close();
	}

	QFile flicenses(":/docs/docs/Licenses.txt");
	if(flicenses.open(QFile::ReadOnly))
	{
		ui->txtLicenses->setText(flicenses.readAll());
		flicenses.close();
	}
}

AboutDialog::~AboutDialog()
{
	delete ui;
}
