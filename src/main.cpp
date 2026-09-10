#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Smart Air Traffic Control Management System");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("ATC Project Team");

    QMainWindow window;
    window.setWindowTitle("Smart Air Traffic Control Management System");
    window.setMinimumSize(1024, 700);

    // Placeholder central widget — will be replaced with full UI in later days
    QWidget *central = new QWidget(&window);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setAlignment(Qt::AlignCenter);

    QLabel *title = new QLabel("Smart Air Traffic Control Management System", central);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);

    QLabel *subtitle = new QLabel(
        "Airport Runway Manager & Resource Allocation\n"
        "B.Tech Project — OS & DBMS Concepts Demonstration\n\n"
        "Phase 1 — Project foundation. Features implemented in subsequent days.",
        central
    );
    subtitle->setAlignment(Qt::AlignCenter);

    layout->addStretch();
    layout->addWidget(title);
    layout->addSpacing(16);
    layout->addWidget(subtitle);
    layout->addStretch();

    window.setCentralWidget(central);
    window.show();

    return app.exec();
}
