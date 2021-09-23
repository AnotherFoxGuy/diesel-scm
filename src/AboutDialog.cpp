#include "AboutDialog.h"
#include "ui_AboutDialog.h"
#include <QFile>

AboutDialog::AboutDialog(QWidget *parent, const QString &fossilVersion) : QDialog(parent), ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    QString banner(QCoreApplication::applicationName() + " " + QCoreApplication::applicationVersion());
    banner += "\n" + ui->lblBanner->text();
    banner += "\n<a href=\"https://fuel-scm.org/\">https://fuel-scm.org/</a>";
    banner = banner.replace("\n", "<br/>");
    ui->lblBanner->setText(banner);
    ui->lblBanner->setOpenExternalLinks(true);
    ui->lblBanner->setTextFormat(Qt::RichText);

    ui->lblQtVersion->setText(tr("QT version %0").arg(QT_VERSION_STR));

    if (!fossilVersion.isEmpty())
        ui->lblFossilVersion->setText(tr("Fossil version %0").arg(fossilVersion));

    QString revisiontxt;
    QFile fmanifest(":/version/manifest");
    if (fmanifest.open(QFile::ReadOnly))
    {
        QString revision = QString(fmanifest.readAll()).trimmed();
        revisiontxt = QString(tr("Fuel revision %0").arg("<a href=\"https://fuel-scm.org/fossil/timeline?c=%0\">%0</a>").arg(revision));
        ui->lblFuelRevision->setOpenExternalLinks(true);
        ui->lblFuelRevision->setTextFormat(Qt::RichText);
        fmanifest.close();
    }
    ui->lblFuelRevision->setText(revisiontxt);

    QString additional;
    QFile ftrans(":/docs/docs/Translators.txt");
    if (ftrans.open(QFile::ReadOnly))
    {
        additional.append(tr("Translations with the help of:") + "\n");
        additional.append(ftrans.readAll());
        additional.append("\n\n");
        ftrans.close();
    }

    QFile flicenses(":/docs/docs/Licenses.txt");
    if (flicenses.open(QFile::ReadOnly))
    {
        additional.append(tr("This sofware uses the following open-source libraries and assets:") + "\n");
        additional.append(flicenses.readAll());
        flicenses.close();
    }
    ui->txtAdditional->setText(additional);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
