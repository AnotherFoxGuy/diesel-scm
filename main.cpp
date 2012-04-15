#include <QtGui/QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
	a.setApplicationName("Fuel");
	a.setApplicationVersion("0.9.6");
	a.setOrganizationDomain("fuel-scm.org");
	a.setOrganizationName("Fuel-SCM");

	// Native OSX applications don't use menu icons
	#ifdef Q_WS_MACX
		a.setAttribute(Qt::AA_DontShowIconsInMenus);
	#endif

	{
		QString workspace;
		if(a.arguments().length()>1)
			workspace = a.arguments()[1];

		MainWindow w(0, &workspace);
		w.show();
		return a.exec();
	}
}
