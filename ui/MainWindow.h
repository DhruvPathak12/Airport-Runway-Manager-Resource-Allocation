#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>

class ApplicationService;
class LoginView;
class FlightsView;
class AircraftView;
class RunwaysView;
class QPushButton;

/**
 * @brief The main application window with authentication flow and RBAC navigation.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ApplicationService *service, QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onNavItemChanged(int row);
    void onUserLoggedIn();
    void onUserLoggedOut();
    void onLogoutClicked();

private:
    void setupUi();
    void setupNavigation();
    void setupPages();
    void applyRolePermissions();
    QWidget* makePlaceholderPage(const QString &title, const QString &description);

    void updateDatabaseStatus();

    // ── UI components ─────────────────────────────────────────────────────────
    QStackedWidget *m_mainStack     = nullptr; // 0: LoginView, 1: Main shell
    LoginView      *m_loginView     = nullptr;
    FlightsView    *m_flightsView   = nullptr;
    AircraftView   *m_aircraftView  = nullptr;
    RunwaysView    *m_runwaysView   = nullptr;
    QWidget        *m_appShell      = nullptr;

    QListWidget    *m_navList       = nullptr;
    QStackedWidget *m_pageStack     = nullptr;
    QLabel         *m_dbStatusLabel = nullptr;
    QLabel         *m_userInfoLabel = nullptr;
    QPushButton    *m_logoutButton  = nullptr;

    // ── Application service reference ─────────────────────────────────────────
    ApplicationService *m_service   = nullptr;
};
