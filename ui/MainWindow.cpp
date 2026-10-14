#include "MainWindow.h"

#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QFont>
#include <QSplitter>

// ─────────────────────────────────────────────────────────────────────────────
// Navigation items: name → placeholder description
// ─────────────────────────────────────────────────────────────────────────────
static const struct {
    const char *label;
    const char *description;
} NAV_ITEMS[] = {
    { "Dashboard",     "Dashboard — real-time overview (Day 24)"                   },
    { "Flights",       "Flight Management — add, update, cancel (Day 7)"           },
    { "Aircraft",      "Aircraft Management — register, maintain (Day 8)"          },
    { "Runways",       "Runway Management — status, allocation (Day 14)"           },
    { "Scheduling",    "Priority Scheduler — ready queue, metrics (Day 11–12)"     },
    { "Transactions",  "Transaction Monitor — COMMIT / ROLLBACK (Day 21–22)"      },
    { "Logs",          "Operation Logs — audit trail (Day 7+)"                     },
    { "Users",         "User Management — roles, auth (Day 6)"                     },
};
static constexpr int NAV_COUNT = sizeof(NAV_ITEMS) / sizeof(NAV_ITEMS[0]);

#include "ui/LoginView.h"
#include "ui/FlightsView.h"
#include <QPushButton>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(ApplicationService *service, QWidget *parent)
    : QMainWindow(parent)
    , m_service(service)
{
    setWindowTitle("Smart Air Traffic Control Management System");
    setMinimumSize(1100, 720);
    setupUi();

    if (m_service) {
        connect(m_service, &ApplicationService::userLoggedIn, this, &MainWindow::onUserLoggedIn);
        connect(m_service, &ApplicationService::userLoggedOut, this, &MainWindow::onUserLoggedOut);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// UI Setup
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::setupUi()
{
    m_mainStack = new QStackedWidget(this);

    // Page 0: Login View
    m_loginView = new LoginView(m_service, this);
    connect(m_loginView, &LoginView::loginSuccessful, this, &MainWindow::onUserLoggedIn);
    m_mainStack->addWidget(m_loginView);

    // Page 1: Main Application Shell
    m_appShell = new QWidget(this);

    // ── Title bar label ──────────────────────────────────────────────────────
    QLabel *titleLabel = new QLabel("✈  Smart Air Traffic Control Management System");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(13);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: #1a1a2e; padding: 6px 12px;");

    m_dbStatusLabel = new QLabel;
    m_dbStatusLabel->setStyleSheet("font-size: 11px; padding: 4px 10px; margin-right: 8px; border-radius: 4px; font-weight: bold;");

    m_userInfoLabel = new QLabel;
    m_userInfoLabel->setStyleSheet("font-size: 11px; color: #1a1a2e; font-weight: bold; padding: 4px 10px; margin-right: 8px; background-color: #e0e7ff; border-radius: 4px;");

    m_logoutButton = new QPushButton(QStringLiteral("Logout"));
    m_logoutButton->setCursor(Qt::PointingHandCursor);
    m_logoutButton->setStyleSheet(R"(
        QPushButton {
            background-color: #c62828;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 4px 12px;
            font-size: 11px;
            font-weight: bold;
            margin-right: 12px;
        }
        QPushButton:hover {
            background-color: #b71c1c;
        }
    )");
    connect(m_logoutButton, &QPushButton::clicked, this, &MainWindow::onLogoutClicked);

    QFrame *titleBar = new QFrame;
    titleBar->setFrameShape(QFrame::NoFrame);
    titleBar->setStyleSheet("background-color: #e8f0fe; border-bottom: 1px solid #c5cae9;");
    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_dbStatusLabel);
    titleLayout->addWidget(m_userInfoLabel);
    titleLayout->addWidget(m_logoutButton);

    updateDatabaseStatus();

    // ── Navigation sidebar ───────────────────────────────────────────────────
    m_navList = new QListWidget;
    m_navList->setFixedWidth(200);
    m_navList->setStyleSheet(R"(
        QListWidget {
            background-color: #1a1a2e;
            color: #e0e0e0;
            font-size: 13px;
            border: none;
            outline: none;
        }
        QListWidget::item {
            padding: 12px 20px;
        }
        QListWidget::item:selected {
            background-color: #3949ab;
            color: #ffffff;
        }
        QListWidget::item:hover:!selected {
            background-color: #283593;
            color: #ffffff;
        }
        QListWidget::item:disabled {
            color: #555577;
            background-color: #141424;
        }
    )");

    // ── Page stack ───────────────────────────────────────────────────────────
    m_pageStack = new QStackedWidget;
    m_pageStack->setStyleSheet("background-color: #f5f7fa;");

    setupNavigation();
    setupPages();

    // ── Main content layout ──────────────────────────────────────────────────
    QWidget *contentArea = new QWidget;
    QHBoxLayout *contentLayout = new QHBoxLayout(contentArea);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(m_navList);
    contentLayout->addWidget(m_pageStack);

    // ── Root layout for app shell ────────────────────────────────────────────
    QVBoxLayout *shellLayout = new QVBoxLayout(m_appShell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    shellLayout->addWidget(titleBar);
    shellLayout->addWidget(contentArea);

    m_mainStack->addWidget(m_appShell);
    setCentralWidget(m_mainStack);

    // Initial view: show login view
    m_mainStack->setCurrentWidget(m_loginView);

    connect(m_navList, &QListWidget::currentRowChanged,
            this,      &MainWindow::onNavItemChanged);
}

void MainWindow::setupNavigation()
{
    for (int i = 0; i < NAV_COUNT; ++i) {
        m_navList->addItem(NAV_ITEMS[i].label);
    }
}

void MainWindow::setupPages()
{
    for (int i = 0; i < NAV_COUNT; ++i) {
        if (i == 1) { // Flights page (Day 7)
            m_flightsView = new FlightsView(m_service, this);
            m_pageStack->addWidget(m_flightsView);
        } else {
            m_pageStack->addWidget(
                makePlaceholderPage(NAV_ITEMS[i].label, NAV_ITEMS[i].description)
            );
        }
    }
}

QWidget* MainWindow::makePlaceholderPage(const QString &title, const QString &description)
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel(title);
    QFont tf = titleLabel->font();
    tf.setPointSize(22);
    tf.setBold(true);
    titleLabel->setFont(tf);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #1a1a2e;");

    QLabel *descLabel = new QLabel(description);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet("color: #555; font-size: 13px;");

    QLabel *noteLabel = new QLabel("[ Placeholder — implementation scheduled for later days ]");
    noteLabel->setAlignment(Qt::AlignCenter);
    noteLabel->setStyleSheet("color: #999; font-size: 11px; margin-top: 8px;");

    layout->addStretch();
    layout->addWidget(titleLabel);
    layout->addSpacing(8);
    layout->addWidget(descLabel);
    layout->addWidget(noteLabel);
    layout->addStretch();

    return page;
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onNavItemChanged(int row)
{
    if (row >= 0 && row < m_pageStack->count()) {
        m_pageStack->setCurrentIndex(row);
        if (row == 1 && m_flightsView) {
            m_flightsView->refreshFlights();
        }
    }
}

void MainWindow::updateDatabaseStatus()
{
    if (!m_dbStatusLabel) {
        return;
    }

    DatabaseManager &db = DatabaseManager::instance();
    DatabaseManager::ConnectionState state = db.connectionState();

    switch (state) {
    case DatabaseManager::ConnectionState::Connected:
        m_dbStatusLabel->setText(QStringLiteral("● DB: Connected (%1@%2)").arg(db.databaseName(), db.host()));
        m_dbStatusLabel->setStyleSheet("color: #155724; background-color: #d4edda; border: 1px solid #c3e6cb; "
                                       "font-size: 11px; padding: 4px 10px; margin-right: 12px; border-radius: 4px; font-weight: bold;");
        break;
    case DatabaseManager::ConnectionState::DriverUnavailable:
        m_dbStatusLabel->setText(QStringLiteral("▲ DB: QMYSQL Driver Missing"));
        m_dbStatusLabel->setStyleSheet("color: #721c24; background-color: #f8d7da; border: 1px solid #f5c6cb; "
                                       "font-size: 11px; padding: 4px 10px; margin-right: 12px; border-radius: 4px; font-weight: bold;");
        break;
    case DatabaseManager::ConnectionState::NotConfigured:
        m_dbStatusLabel->setText(QStringLiteral("○ DB: Not Configured (Offline)"));
        m_dbStatusLabel->setStyleSheet("color: #856404; background-color: #fff3cd; border: 1px solid #ffeeba; "
                                       "font-size: 11px; padding: 4px 10px; margin-right: 12px; border-radius: 4px; font-weight: bold;");
        break;
    case DatabaseManager::ConnectionState::AuthFailed:
        m_dbStatusLabel->setText(QStringLiteral("✕ DB: Auth Failed"));
        m_dbStatusLabel->setStyleSheet("color: #721c24; background-color: #f8d7da; border: 1px solid #f5c6cb; "
                                       "font-size: 11px; padding: 4px 10px; margin-right: 12px; border-radius: 4px; font-weight: bold;");
        break;
    case DatabaseManager::ConnectionState::ServerUnreachable:
        m_dbStatusLabel->setText(QStringLiteral("✕ DB: Server Unreachable"));
        m_dbStatusLabel->setStyleSheet("color: #721c24; background-color: #f8d7da; border: 1px solid #f5c6cb; "
                                       "font-size: 11px; padding: 4px 10px; margin-right: 12px; border-radius: 4px; font-weight: bold;");
        break;
    case DatabaseManager::ConnectionState::DatabaseNotFound:
        m_dbStatusLabel->setText(QStringLiteral("✕ DB: Schema Not Found"));
        m_dbStatusLabel->setStyleSheet("color: #721c24; background-color: #f8d7da; border: 1px solid #f5c6cb; "
                                       "font-size: 11px; padding: 4px 10px; margin-right: 12px; border-radius: 4px; font-weight: bold;");
        break;
    case DatabaseManager::ConnectionState::Disconnected:
    default:
        m_dbStatusLabel->setText(QStringLiteral("○ DB: Disconnected"));
        m_dbStatusLabel->setStyleSheet("color: #383d41; background-color: #e2e3e5; border: 1px solid #d6d8db; "
                                       "font-size: 11px; padding: 4px 10px; margin-right: 12px; border-radius: 4px; font-weight: bold;");
        break;
    }
}

void MainWindow::onUserLoggedIn()
{
    if (!m_service || !m_service->isAuthenticated()) {
        return;
    }

    const Session &session = m_service->currentSession();
    QString displayName = session.fullName().isEmpty() ? session.username() : session.fullName();
    m_userInfoLabel->setText(QStringLiteral("👤 %1 (%2)").arg(displayName, session.role()));

    applyRolePermissions();
    updateDatabaseStatus();

    if (m_flightsView) {
        m_flightsView->updatePermissions();
        m_flightsView->refreshFlights();
    }

    m_mainStack->setCurrentWidget(m_appShell);
}

void MainWindow::onUserLoggedOut()
{
    m_userInfoLabel->clear();
    m_loginView->resetForm();
    m_mainStack->setCurrentWidget(m_loginView);
}

void MainWindow::onLogoutClicked()
{
    if (m_service) {
        m_service->logout();
    }
}

void MainWindow::applyRolePermissions()
{
    if (!m_service || !m_service->isAuthenticated()) {
        return;
    }

    const Session &session = m_service->currentSession();

    // Iterate through all navigation items and filter based on Session permissions
    int firstAccessibleRow = -1;
    for (int i = 0; i < NAV_COUNT && i < m_navList->count(); ++i) {
        QString label = NAV_ITEMS[i].label;
        QString permKey = QStringLiteral("page:%1").arg(label);

        bool allowed = session.hasPermission(permKey);
        QListWidgetItem *item = m_navList->item(i);
        if (item) {
            item->setHidden(!allowed);
            if (allowed && firstAccessibleRow == -1) {
                firstAccessibleRow = i;
            }
        }
    }

    if (firstAccessibleRow != -1) {
        m_navList->setCurrentRow(firstAccessibleRow);
        m_pageStack->setCurrentIndex(firstAccessibleRow);
    }
}
