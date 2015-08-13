#include "AboutDialog.h"
#include "ui_AboutDialog.h"
#include <QFile>

AboutDialog::AboutDialog(QWidget *parent, const QString &fossilVersion) :
	QDialog(parent),
	ui(new Ui::AboutDialog)
{
	ui->setupUi(this);
	QString banner(QCoreApplication::applicationName() + " " + QCoreApplication::applicationVersion());
	ui->lblBanner->setText(banner + "\n" + ui->lblBanner->text());

	ui->lblQtVersion->setText(tr("QT version %0").arg(QT_VERSION_STR));

	if(!fossilVersion.isEmpty())
		ui->lblFossilVersion->setText(tr("Fossil version %0").arg(fossilVersion));

	QString additional;
	QFile ftrans(":/docs/docs/Translators.txt");
	if(ftrans.open(QFile::ReadOnly))
	{
		additional.append(tr("Translations with the help of:")+"\n");
		additional.append(ftrans.readAll());
		additional.append("\n\n");
		ftrans.close();
	}

	QFile flicenses(":/docs/docs/Licenses.txt");
	if(flicenses.open(QFile::ReadOnly))
	{
		additional.append(tr("This sofware uses the following open-source libraries and assets:")+"\n");
		additional.append(flicenses.readAll());
		flicenses.close();
	}
	ui->txtAdditional->setText(additional);
}

AboutDialog::~AboutDialog()
{
	delete ui;
}
