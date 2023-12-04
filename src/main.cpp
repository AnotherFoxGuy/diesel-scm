#include "MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Diesel");
    app.setApplicationVersion(DIESEL_VERSION);
    app.setOrganizationDomain("diesel-scm.org");
    app.setOrganizationName("Diesel-SCM");

#ifdef Q_OS_MACX
    // Native OSX applications don't have menu icons
    app.setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
    {
        bool portable = false;
        QString workspace;

        Q_ASSERT(app.arguments().size() > 0);
        for (int i = 1; i < app.arguments().size(); ++i)
        {
            QString arg = app.arguments()[i];
            // Parse options
            if (arg.indexOf("--") != -1)
            {
                if (arg.indexOf("portable") != -1)
                    portable = true;
                continue;
            }
            else
                workspace = arg;
        }

        Settings settings(portable);

        MainWindow mainwin(settings, 0, workspace.isEmpty() ? 0 : &workspace);
        mainwin.show();
        mainwin.fullRefresh();
        return app.exec();
    }
}
