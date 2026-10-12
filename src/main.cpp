#include <QApplication>

#include "services/ApplicationService.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Smart Air Traffic Control Management System");
    app.setApplicationVersion("0.2.0");
    app.setOrganizationName("ATC Project Team");

    // Initialize application service layer
    ApplicationService service;
    service.initialize();

    // Create and show the main window
    MainWindow window(&service);
    window.show();

    int result = app.exec();

    // Clean shutdown
    service.shutdown();

    return result;
}
