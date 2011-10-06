#include <QtGui/QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
	a.setApplicationName("Fuel");
	a.setApplicationVersion("0.9.4");
	a.setOrganizationDomain("karanik.com");
	a.setOrganizationName("Karanik");

	// Native OSX applications don't use menu icons
	#ifdef Q_WS_MACX
		a.setAttribute(Qt::AA_DontShowIconsInMenus);
	#endif

	{
		MainWindow w;
		w.show();
		return a.exec();
	}
}
