#include <QtGui/QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
	a.setApplicationName("Fuel");
	a.setApplicationVersion("0.9.0");
	a.setOrganizationDomain("karanik.com");
	a.setOrganizationName("karanik");
	MainWindow w;
    w.show();

    return a.exec();
}
