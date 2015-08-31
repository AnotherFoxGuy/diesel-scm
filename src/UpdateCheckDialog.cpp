#include "UpdateCheckDialog.h"
#include "ui_UpdateCheckDialog.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include "Utils.h"

//-----------------------------------------------------------------------------
UpdateCheckDialog::UpdateCheckDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::UpdateCheckDialog)
{
	ui->setupUi(this);

	QString text = tr("Current Version: %0").arg(QCoreApplication::applicationVersion());
	text += "\n" + tr("Latest Version: %0").arg(tr("Checking...")) + "\n";
	ui->lblCurrentVersion->setText(text);

	connect(
	 &networkAccess, SIGNAL (finished(QNetworkReply*)),
	 this, SLOT (versionInfoDownloaded(QNetworkReply*))
	 );

	QNetworkRequest request(QUrl("https://fuel-scm.org/files/releases/VersionCheck"));
	networkAccess.get(request);
}

//-----------------------------------------------------------------------------
UpdateCheckDialog::~UpdateCheckDialog()
{
	delete ui;
}

//-----------------------------------------------------------------------------
void UpdateCheckDialog::versionInfoDownloaded(QNetworkReply *reply)
{
	if(!reply)
	{
		ui->lblCurrentVersion->setText(tr("Error: %0").arg(tr("Could not connect to server")));
		return;
	}

	reply->deleteLater();

	if(reply && reply->error() != QNetworkReply::NoError)
	{
		ui->lblCurrentVersion->setText(tr("Error: %0").arg(reply->errorString()));
		return;
	}

	QByteArray data = reply->readAll();

	QStringMap props;
	ParseProperties(props, QString(data).split('\n'), '=');
	const QString lastest_key = QCoreApplication::applicationName() + ".Latest";

	if(props.find(lastest_key)==props.end())
	{
		ui->lblCurrentVersion->setText(tr("Error: %0").arg(tr("Invalid format")));
		return;
	}

	Version vcurr(QCoreApplication::applicationVersion());
	Version vlatest(props[lastest_key]);

	QString text = tr("Current Version: %0").arg(QCoreApplication::applicationVersion());

	text += "\n" + tr("Latest Version: %0").arg(props[lastest_key]);

	QString changelog_url;
	if(vcurr<vlatest)
	{
		text += "\n" + tr("A new version is available");

		const QString changelog_key = QCoreApplication::applicationName() + ".ChangeLog";

		if(props.find(changelog_key)!=props.end())
			changelog_url = props[changelog_key];
	}
	else if(vcurr==vlatest)
		text += "\n" + tr("This is the latest version");
	else
		text += "\n" + tr("You are running an unreleased version");

	ui->lblCurrentVersion->setText(text);

	// If there is a changelog, fire download
	if(!changelog_url.isEmpty())
	{
		// Prepare to download changelog
		disconnect(
			&networkAccess, SIGNAL (finished(QNetworkReply*)),
			this, SLOT (versionInfoDownloaded(QNetworkReply*))
		);

		connect(
		 &networkAccess, SIGNAL (finished(QNetworkReply*)),
		 this, SLOT (changelogDownloaded(QNetworkReply*))
		 );

		QNetworkRequest request(changelog_url);
		networkAccess.get(request);
	}
}

//-----------------------------------------------------------------------------
void UpdateCheckDialog::changelogDownloaded(QNetworkReply *reply)
{
	if(!reply)
		return;

	reply->deleteLater();

	if(reply && reply->error() != QNetworkReply::NoError)
		return;

	QByteArray data = reply->readAll();
	if(data.size()==0)
		return;

	QString txt(data);

	// HACK: Remove anchors from text
	QRegExp rx("<a[^>]*>([^<]+)</a>");
	txt = txt.remove(rx);

	ui->txtChanges->setText(txt);
}
