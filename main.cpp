#include <QApplication>
#include <QIcon>
#include <QFileInfo>
#include <QDebug>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    qWarning() << "=== PROGRAM STARTED ===";
    QApplication app(argc, argv);

    QIcon appIcon(":icon/DHNP.png");

    if (appIcon.isNull())
    {
        qWarning()<<"Err: Application icon file can not be found in dat/icn/ forlder!";
    }

    else
    {
        //  Path for icon.
        app.setWindowIcon(QIcon(":icon/DHNP.png"));
    }


    app.setDesktopFileName("DHNP");
    app.setApplicationName("Die Hard: Nakatomi Plaza Modding Tools V1.0");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Modding Tools");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
