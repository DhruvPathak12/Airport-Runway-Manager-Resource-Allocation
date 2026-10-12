#include "MainWindow.h"

#include "services/ApplicationService.h"

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
}

// ─────────────────────────────────────────────────────────────────────────────
// UI Setup
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::setupUi()
{
    // ── Title bar label ──────────────────────────────────────────────────────
    QLabel *titleLabel = new QLabel("✈  Smart Air Traffic Control Management System");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(13);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: #1a1a2e; padding: 6px 12px;");

    QFrame *titleBar = new QFrame;
    titleBar->setFrameShape(QFrame::NoFrame);
    titleBar->setStyleSheet("background-color: #e8f0fe; border-bottom: 1px solid #c5cae9;");
    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

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

    // ── Root layout ──────────────────────────────────────────────────────────
    QWidget *root = new QWidget;
    QVBoxLayout *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(titleBar);
    rootLayout->addWidget(contentArea);

    setCentralWidget(root);

    // Select the first navigation item by default
    m_navList->setCurrentRow(0);

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
        m_pageStack->addWidget(
            makePlaceholderPage(NAV_ITEMS[i].label, NAV_ITEMS[i].description)
        );
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
    }
}
