#include "RunwaysView.h"

#include "services/ApplicationService.h"
#include "resource/ResourceManager.h"
#include "ui/RunwayDialog.h"
#include "ui/RunwayAllocationDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>

RunwaysView::RunwaysView(ApplicationService *service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
{
    setupUi();

    if (m_service) {
        connect(m_service, &ApplicationService::runwaysChanged, this, &RunwaysView::refreshRunways);

        if (m_service->resourceManager()) {
            m_demoEngine = new ConcurrentAllocationDemo(m_service->resourceManager(), this);
            connect(m_demoEngine, &ConcurrentAllocationDemo::demoStarted, this, &RunwaysView::onDemoStarted);
            connect(m_demoEngine, &ConcurrentAllocationDemo::threadStateChanged, this, &RunwaysView::onThreadStateChanged);
            connect(m_demoEngine, &ConcurrentAllocationDemo::demoFinished, this, &RunwaysView::onDemoFinished);

            connect(m_service->resourceManager(), &ResourceManager::criticalSectionEntered,
                    this, [this](int runwayId, int flightId) {
                        if (m_csBanner) {
                            m_csBanner->setText(QStringLiteral("🔒 [Runway %1] Mutex Acquired | Critical Section Entered%2")
                                .arg(runwayId)
                                .arg(flightId > 0 ? QStringLiteral(" for Flight #%1").arg(flightId) : QString()));
                            m_csBanner->setStyleSheet(QStringLiteral(
                                "background-color: #fefce8; color: #854d0e; border: 1px solid #fef08a; border-radius: 6px; padding: 8px 12px; font-size: 12px; font-weight: 500;"));
                        }
                    });

            connect(m_service->resourceManager(), &ResourceManager::runwayReserved,
                    this, [this](int runwayId, int flightId) {
                        if (m_csBanner) {
                            m_csBanner->setText(QStringLiteral("✅ [Runway %1] Resource Reserved for Flight #%2 | Mutual Exclusion Enforced")
                                .arg(runwayId).arg(flightId));
                            m_csBanner->setStyleSheet(QStringLiteral(
                                "background-color: #eff6ff; color: #1e40af; border: 1px solid #bfdbfe; border-radius: 6px; padding: 8px 12px; font-size: 12px; font-weight: 500;"));
                        }
                        refreshRunways();
                    });

            connect(m_service->resourceManager(), &ResourceManager::runwayReleased,
                    this, [this](int runwayId) {
                        if (m_csBanner) {
                            m_csBanner->setText(QStringLiteral("🔓 [Runway %1] Critical Section Released | Resource Returned to Available")
                                .arg(runwayId));
                            m_csBanner->setStyleSheet(QStringLiteral(
                                "background-color: #f0fdf4; color: #166534; border: 1px solid #bbf7d0; border-radius: 6px; padding: 8px 12px; font-size: 12px; font-weight: 500;"));
                        }
                        refreshRunways();
                    });

            connect(m_service->resourceManager(), &ResourceManager::allocationContentionRejected,
                    this, [this](int runwayId, int flightId, const QString &reason) {
                        Q_UNUSED(flightId)
                        if (m_csBanner) {
                            m_csBanner->setText(QStringLiteral("⚠️ [Runway %1] Contention Conflict: %2")
                                .arg(runwayId).arg(reason));
                            m_csBanner->setStyleSheet(QStringLiteral(
                                "background-color: #fef2f2; color: #991b1b; border: 1px solid #fecaca; border-radius: 6px; padding: 8px 12px; font-size: 12px; font-weight: 500;"));
                        }
                        refreshRunways();
                    });
        }
    }
}

RunwaysView::~RunwaysView()
{
    if (m_demoEngine) {
        m_demoEngine->cancelAndJoin();
    }
}

void RunwaysView::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #cbd5e1;
            border-radius: 8px;
            background-color: #ffffff;
            top: -1px;
        }
        QTabBar::tab {
            background-color: #f1f5f9;
            color: #475569;
            padding: 8px 18px;
            font-size: 13px;
            font-weight: 600;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            border: 1px solid #cbd5e1;
            margin-right: 4px;
        }
        QTabBar::tab:selected {
            background-color: #ffffff;
            color: #0f172a;
            border-bottom: 2px solid #2563eb;
        }
        QTabBar::tab:hover:!selected {
            background-color: #e2e8f0;
        }
    )");

    m_tabWidget->addTab(createLiveTab(), QStringLiteral("🛫 Live Runways & Critical Section"));
    m_tabWidget->addTab(createConcurrentTab(), QStringLiteral("🔬 Concurrent Allocation Demonstration (C++ Threads)"));

    mainLayout->addWidget(m_tabWidget);
}

QWidget* RunwaysView::createConcurrentTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(16, 16, 16, 16);
    tabLayout->setSpacing(14);

    // ── 1. Header Row ────────────────────────────────────────────────────────
    QVBoxLayout *headerLayout = new QVBoxLayout;
    QLabel *titleLabel = new QLabel(QStringLiteral("Concurrent Runway Allocation Demonstration (Day 20)"), tab);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #0f172a;");

    QLabel *subLabel = new QLabel(QStringLiteral(
        "Simulates multiple concurrent C++ threads (std::thread) competing for a single shared runway resource simultaneously "
        "via a start barrier (std::condition_variable). Demonstrates in-process mutual exclusion, race condition prevention, and safe thread lifecycle management."), tab);
    subLabel->setStyleSheet("font-size: 13px; color: #64748b;");
    subLabel->setWordWrap(true);

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subLabel);
    tabLayout->addLayout(headerLayout);

    // ── 2. Controls Toolbar ──────────────────────────────────────────────────
    QHBoxLayout *controlsLayout = new QHBoxLayout;
    controlsLayout->setSpacing(12);

    QLabel *targetLabel = new QLabel(QStringLiteral("Target Runway:"), tab);
    targetLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #334155;");
    controlsLayout->addWidget(targetLabel);

    m_demoRunwayCombo = new QComboBox(tab);
    m_demoRunwayCombo->setMinimumWidth(220);
    m_demoRunwayCombo->setStyleSheet(R"(
        QComboBox {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
            color: #0f172a;
        }
    )");
    controlsLayout->addWidget(m_demoRunwayCombo);

    QLabel *threadsLabel = new QLabel(QStringLiteral("Worker Threads:"), tab);
    threadsLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #334155; margin-left: 8px;");
    controlsLayout->addWidget(threadsLabel);

    m_demoThreadSpin = new QSpinBox(tab);
    m_demoThreadSpin->setRange(2, 10);
    m_demoThreadSpin->setValue(3);
    m_demoThreadSpin->setMinimumWidth(70);
    m_demoThreadSpin->setStyleSheet(R"(
        QSpinBox {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 13px;
            color: #0f172a;
        }
    )");
    controlsLayout->addWidget(m_demoThreadSpin);

    m_startDemoBtn = new QPushButton(QStringLiteral("▶ Start Concurrent Test"), tab);
    m_startDemoBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #2563eb;
            color: #ffffff;
            font-weight: 600;
            padding: 8px 18px;
            border-radius: 6px;
            border: none;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #1d4ed8;
        }
        QPushButton:disabled {
            background-color: #93c5fd;
            color: #ffffff;
        }
    )");
    connect(m_startDemoBtn, &QPushButton::clicked, this, &RunwaysView::onStartConcurrentTestClicked);
    controlsLayout->addWidget(m_startDemoBtn);

    m_resetDemoBtn = new QPushButton(QStringLiteral("↺ Reset Demo"), tab);
    m_resetDemoBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #f1f5f9;
            border: 1px solid #cbd5e1;
            color: #334155;
            font-weight: 600;
            padding: 8px 14px;
            border-radius: 6px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #e2e8f0;
            border-color: #94a3b8;
        }
    )");
    connect(m_resetDemoBtn, &QPushButton::clicked, this, &RunwaysView::onResetDemoClicked);
    controlsLayout->addWidget(m_resetDemoBtn);

    m_refreshDemoBtn = new QPushButton(QStringLiteral("🔄 Refresh Resource State"), tab);
    m_refreshDemoBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            color: #334155;
            font-weight: 600;
            padding: 8px 14px;
            border-radius: 6px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #f8fafc;
            border-color: #94a3b8;
        }
    )");
    connect(m_refreshDemoBtn, &QPushButton::clicked, this, &RunwaysView::onRefreshResourceStateClicked);
    controlsLayout->addWidget(m_refreshDemoBtn);

    controlsLayout->addStretch();
    tabLayout->addLayout(controlsLayout);

    // ── 3. Status Banner ─────────────────────────────────────────────────────
    m_demoStatusLabel = new QLabel(QStringLiteral("ℹ️ Ready to launch concurrent threads. Select a runway and click 'Start Concurrent Test'."), tab);
    m_demoStatusLabel->setStyleSheet(R"(
        QLabel {
            background-color: #f8fafc;
            color: #334155;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            padding: 10px 14px;
            font-size: 13px;
            font-weight: 500;
        }
    )");
    tabLayout->addWidget(m_demoStatusLabel);

    // ── 4. Metrics Cards Dashboard ───────────────────────────────────────────
    QHBoxLayout *cardsLayout = new QHBoxLayout;
    cardsLayout->setSpacing(12);

    auto makeCard = [tab](const QString &title, QLabel *&valLabel, const QString &initVal, const QString &accentColor) -> QWidget* {
        QWidget *card = new QWidget(tab);
        card->setStyleSheet(QStringLiteral(
            "background-color: #ffffff; border: 1px solid #e2e8f0; border-top: 3px solid %1; border-radius: 6px; padding: 8px 12px;")
            .arg(accentColor));
        QVBoxLayout *cLayout = new QVBoxLayout(card);
        cLayout->setContentsMargins(4, 4, 4, 4);
        cLayout->setSpacing(2);

        QLabel *tLabel = new QLabel(title, card);
        tLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #64748b; text-transform: uppercase;");

        valLabel = new QLabel(initVal, card);
        valLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: %1;").arg(accentColor));

        cLayout->addWidget(tLabel);
        cLayout->addWidget(valLabel);
        return card;
    };

    cardsLayout->addWidget(makeCard(QStringLiteral("Total Requests"), m_totalReqCard, QStringLiteral("0 Requests"), QStringLiteral("#3b82f6")));
    cardsLayout->addWidget(makeCard(QStringLiteral("Successful Allocations"), m_successCard, QStringLiteral("0 Winner"), QStringLiteral("#16a34a")));
    cardsLayout->addWidget(makeCard(QStringLiteral("Rejected Allocations"), m_rejectedCard, QStringLiteral("0 Conflicts"), QStringLiteral("#dc2626")));
    cardsLayout->addWidget(makeCard(QStringLiteral("Final Runway Owner"), m_winnerCard, QStringLiteral("None (Available)"), QStringLiteral("#0f172a")));

    tabLayout->addLayout(cardsLayout);

    // ── 5. Results Table ─────────────────────────────────────────────────────
    m_demoTable = new QTableWidget(tab);
    m_demoTable->setColumnCount(7);
    m_demoTable->setHorizontalHeaderLabels({
        QStringLiteral("Thread Label"),
        QStringLiteral("OS System Thread ID"),
        QStringLiteral("Flight ID / Number"),
        QStringLiteral("Requested Runway"),
        QStringLiteral("Request Status"),
        QStringLiteral("Allocation Result"),
        QStringLiteral("Outcome / Contention Message")
    });

    m_demoTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_demoTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_demoTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_demoTable->verticalHeader()->setVisible(false);
    m_demoTable->setAlternatingRowColors(true);
    m_demoTable->setShowGrid(false);

    m_demoTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_demoTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_demoTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_demoTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_demoTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_demoTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_demoTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);

    m_demoTable->setStyleSheet(R"(
        QTableWidget {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 8px;
            gridline-color: transparent;
        }
        QTableWidget::item {
            padding: 8px 12px;
            color: #1e293b;
            border-bottom: 1px solid #f1f5f9;
        }
        QTableWidget::item:selected {
            background-color: #e0e7ff;
            color: #1e1b4b;
        }
        QHeaderView::section {
            background-color: #f8fafc;
            color: #475569;
            font-weight: 600;
            font-size: 12px;
            padding: 10px 12px;
            border: none;
            border-bottom: 2px solid #cbd5e1;
        }
    )");

    tabLayout->addWidget(m_demoTable);

    return tab;
}

QWidget* RunwaysView::createLiveTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(tab);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(14);

    // ── 1. Header Row ────────────────────────────────────────────────────────
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QVBoxLayout *titleTextLayout = new QVBoxLayout;

    QLabel *titleLabel = new QLabel(QStringLiteral("Runway Resource Management"), this);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #0f172a;");

    QLabel *subLabel = new QLabel(QStringLiteral("Monitor shared runway resources, operational availability, and maintenance controls in MySQL"), this);
    subLabel->setStyleSheet("font-size: 13px; color: #64748b;");

    titleTextLayout->addWidget(titleLabel);
    titleTextLayout->addWidget(subLabel);
    headerLayout->addLayout(titleTextLayout);
    headerLayout->addStretch();

    m_countBadge = new QLabel(QStringLiteral("0 Runways"), this);
    m_countBadge->setStyleSheet("background-color: #e2e8f0; color: #334155; font-size: 12px; font-weight: 600; "
                               "padding: 6px 14px; border-radius: 12px; border: 1px solid #cbd5e1;");
    headerLayout->addWidget(m_countBadge);

    m_permNotice = new QLabel(this);
    m_permNotice->setStyleSheet("background-color: #fef3c7; color: #92400e; font-size: 11px; font-weight: 600; "
                               "padding: 6px 12px; border-radius: 12px; border: 1px solid #fde68a;");
    m_permNotice->setText(QStringLiteral("👁 Read-Only"));
    m_permNotice->hide();
    headerLayout->addWidget(m_permNotice);

    mainLayout->addLayout(headerLayout);

    // ── 1b. OS Mutual Exclusion & Critical Section Activity Banner ───────────
    m_csBanner = new QLabel(QStringLiteral("🔒 Mutual Exclusion: Active | Shared runway resources protected by RAII mutex"), this);
    m_csBanner->setStyleSheet(R"(
        QLabel {
            background-color: #f0fdf4;
            color: #166534;
            border: 1px solid #bbf7d0;
            border-radius: 6px;
            padding: 8px 14px;
            font-size: 12px;
            font-weight: 500;
        }
    )");
    mainLayout->addWidget(m_csBanner);

    // ── 2. Toolbar Row ───────────────────────────────────────────────────────
    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    toolbarLayout->setSpacing(10);

    // Search input
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("🔍 Search by Runway Designation or Type..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(320);
    m_searchEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
            color: #0f172a;
        }
        QLineEdit:focus {
            border: 1px solid #3b82f6;
            background-color: #f8fafc;
        }
    )");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &RunwaysView::onSearchChanged);
    toolbarLayout->addWidget(m_searchEdit);

    // Status filter
    m_statusFilter = new QComboBox(this);
    m_statusFilter->addItem(QStringLiteral("All Statuses"));
    m_statusFilter->addItems({
        QStringLiteral("Available"),
        QStringLiteral("Occupied"),
        QStringLiteral("Maintenance")
    });
    m_statusFilter->setStyleSheet(R"(
        QComboBox {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
            color: #0f172a;
            min-width: 140px;
        }
    )");
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RunwaysView::onStatusFilterChanged);
    toolbarLayout->addWidget(m_statusFilter);

    // Refresh button
    m_refreshButton = new QPushButton(QStringLiteral("🔄 Refresh"), this);
    m_refreshButton->setStyleSheet(R"(
        QPushButton {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 14px;
            font-size: 13px;
            font-weight: 500;
            color: #334155;
        }
        QPushButton:hover {
            background-color: #f1f5f9;
            border-color: #94a3b8;
        }
    )");
    connect(m_refreshButton, &QPushButton::clicked, this, &RunwaysView::refreshRunways);
    toolbarLayout->addWidget(m_refreshButton);

    toolbarLayout->addStretch();

    // Allocate Flight Button (ATC only)
    m_allocateButton = new QPushButton(QStringLiteral("🛬 Allocate Flight"), this);
    m_allocateButton->setEnabled(false);
    m_allocateButton->setStyleSheet(R"(
        QPushButton {
            background-color: #059669;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #047857;
        }
        QPushButton:disabled {
            background-color: #cbd5e1;
            color: #94a3b8;
        }
    )");
    connect(m_allocateButton, &QPushButton::clicked, this, &RunwaysView::onAllocateClicked);
    toolbarLayout->addWidget(m_allocateButton);

    // Release Runway Button (ATC, Operator, Admin)
    m_releaseButton = new QPushButton(QStringLiteral("🛫 Release Runway"), this);
    m_releaseButton->setEnabled(false);
    m_releaseButton->setStyleSheet(R"(
        QPushButton {
            background-color: #0284c7;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #0369a1;
        }
        QPushButton:disabled {
            background-color: #cbd5e1;
            color: #94a3b8;
        }
    )");
    connect(m_releaseButton, &QPushButton::clicked, this, &RunwaysView::onReleaseClicked);
    toolbarLayout->addWidget(m_releaseButton);

    // Toggle Maintenance Button (Airport Operator & Admin)
    m_toggleMaintenanceBtn = new QPushButton(QStringLiteral("🛠 Set Maintenance"), this);
    m_toggleMaintenanceBtn->setEnabled(false);
    m_toggleMaintenanceBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #d97706;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #b45309;
        }
        QPushButton:disabled {
            background-color: #cbd5e1;
            color: #94a3b8;
        }
    )");
    connect(m_toggleMaintenanceBtn, &QPushButton::clicked, this, &RunwaysView::onToggleMaintenanceClicked);
    toolbarLayout->addWidget(m_toggleMaintenanceBtn);

    // Add button (Admin only)
    m_addButton = new QPushButton(QStringLiteral("➕ Add Runway"), this);
    m_addButton->setStyleSheet(R"(
        QPushButton {
            background-color: #16a34a;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #15803d;
        }
        QPushButton:disabled {
            background-color: #94a3b8;
        }
    )");
    connect(m_addButton, &QPushButton::clicked, this, &RunwaysView::onAddRunwayClicked);
    toolbarLayout->addWidget(m_addButton);

    // Edit button (Admin or Operator)
    m_editButton = new QPushButton(QStringLiteral("✏ Edit"), this);
    m_editButton->setEnabled(false);
    m_editButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2563eb;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #1d4ed8;
        }
        QPushButton:disabled {
            background-color: #cbd5e1;
            color: #94a3b8;
        }
    )");
    connect(m_editButton, &QPushButton::clicked, this, &RunwaysView::onEditRunwayClicked);
    toolbarLayout->addWidget(m_editButton);

    mainLayout->addLayout(toolbarLayout);

    // ── 3. Content Area (Stacked: 0=Table, 1=Empty/Error State) ──────────────
    m_contentStack = new QStackedWidget(this);

    // Table view
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(6);
    m_tableWidget->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QStringLiteral("Runway Designation"),
        QStringLiteral("Permitted Operations"),
        QStringLiteral("Operational Status"),
        QStringLiteral("Current Allocation / Occupancy"),
        QStringLiteral("OS Critical Section State")
    });

    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setShowGrid(false);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    m_tableWidget->setStyleSheet(R"(
        QTableWidget {
            background-color: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            gridline-color: #f1f5f9;
            font-size: 13px;
        }
        QTableWidget::item {
            padding: 8px 12px;
            color: #1e293b;
            border-bottom: 1px solid #f1f5f9;
        }
        QTableWidget::item:selected {
            background-color: #e0e7ff;
            color: #1e1b4b;
        }
        QHeaderView::section {
            background-color: #f8fafc;
            color: #475569;
            font-weight: 600;
            font-size: 12px;
            padding: 10px 12px;
            border: none;
            border-bottom: 2px solid #cbd5e1;
        }
    )");

    connect(m_tableWidget, &QTableWidget::itemSelectionChanged, this, &RunwaysView::onSelectionChanged);
    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, &RunwaysView::onTableDoubleClicked);

    m_contentStack->addWidget(m_tableWidget);

    // Empty / Error State Widget
    m_stateWidget = new QWidget(this);
    QVBoxLayout *stateLayout = new QVBoxLayout(m_stateWidget);
    stateLayout->setAlignment(Qt::AlignCenter);

    m_stateIcon = new QLabel(QStringLiteral("🛫"), this);
    m_stateIcon->setStyleSheet("font-size: 48px; color: #94a3b8;");
    m_stateIcon->setAlignment(Qt::AlignCenter);

    m_stateTitle = new QLabel(QStringLiteral("No Runways Found"), this);
    m_stateTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #334155; margin-top: 8px;");
    m_stateTitle->setAlignment(Qt::AlignCenter);

    m_stateMessage = new QLabel(QStringLiteral("No runways match your search criteria or the database is unavailable."), this);
    m_stateMessage->setStyleSheet("font-size: 13px; color: #64748b; margin-top: 4px;");
    m_stateMessage->setAlignment(Qt::AlignCenter);

    m_stateButton = new QPushButton(QStringLiteral("Refresh Runways"), this);
    m_stateButton->setStyleSheet("background-color: #2563eb; color: #ffffff; border-radius: 6px; padding: 8px 16px; margin-top: 14px; font-weight: 600;");
    connect(m_stateButton, &QPushButton::clicked, this, &RunwaysView::refreshRunways);

    stateLayout->addWidget(m_stateIcon);
    stateLayout->addWidget(m_stateTitle);
    stateLayout->addWidget(m_stateMessage);
    stateLayout->addWidget(m_stateButton);

    m_contentStack->addWidget(m_stateWidget);
    mainLayout->addWidget(m_contentStack);
    return tab;
}

void RunwaysView::updatePermissions()
{
    if (!m_service) {
        return;
    }

    const Session &session = m_service->currentSession();
    bool canMaintenance = session.hasPermission(QStringLiteral("action:RunwayMaintenance"));
    bool canAllocate = session.hasPermission(QStringLiteral("action:RunwayAllocation"));
    bool isAdmin = session.isAdmin();

    m_addButton->setVisible(isAdmin);
    m_editButton->setVisible(isAdmin || canMaintenance);
    m_toggleMaintenanceBtn->setVisible(canMaintenance || isAdmin);
    m_allocateButton->setVisible(canAllocate);
    m_releaseButton->setVisible(canAllocate || canMaintenance || isAdmin);

    if (isAdmin) {
        m_permNotice->setText(QStringLiteral("👑 Admin Mode"));
        m_permNotice->setStyleSheet("background-color: #dbeafe; color: #1e40af; font-size: 11px; font-weight: 600; padding: 6px 12px; border-radius: 12px; border: 1px solid #bfdbfe;");
        m_permNotice->show();
    } else if (canAllocate) {
        m_permNotice->setText(QStringLiteral("✈ ATC Controller Mode (Runway Allocation Authorized)"));
        m_permNotice->setStyleSheet("background-color: #dcfce7; color: #166534; font-size: 11px; font-weight: 600; padding: 6px 12px; border-radius: 12px; border: 1px solid #bbf7d0;");
        m_permNotice->show();
    } else if (canMaintenance) {
        m_permNotice->setText(QStringLiteral("🛠 Operator Mode (Maintenance Authorized)"));
        m_permNotice->setStyleSheet("background-color: #fef3c7; color: #92400e; font-size: 11px; font-weight: 600; padding: 6px 12px; border-radius: 12px; border: 1px solid #fde68a;");
        m_permNotice->show();
    } else {
        m_permNotice->setText(QStringLiteral("👁 Monitor Mode"));
        m_permNotice->setStyleSheet("background-color: #f1f5f9; color: #475569; font-size: 11px; font-weight: 600; padding: 6px 12px; border-radius: 12px; border: 1px solid #e2e8f0;");
        m_permNotice->show();
    }

    bool canRunDemo = canAllocate || isAdmin;
    if (m_startDemoBtn) {
        m_startDemoBtn->setEnabled(canRunDemo);
        if (!canRunDemo) {
            m_startDemoBtn->setToolTip(QStringLiteral("Requires ATC Controller or Admin privileges."));
        } else {
            m_startDemoBtn->setToolTip(QStringLiteral("Launch concurrent worker threads competing for selected runway."));
        }
    }

    onSelectionChanged();
}

void RunwaysView::refreshRunways()
{
    if (!m_service || !m_service->isAuthenticated()) {
        m_currentRunways.clear();
        m_tableWidget->setRowCount(0);
        m_stateTitle->setText(QStringLiteral("Authentication Required"));
        m_stateMessage->setText(QStringLiteral("Please sign in with authorized credentials to view runway data."));
        m_contentStack->setCurrentWidget(m_stateWidget);
        m_countBadge->setText(QStringLiteral("0 Runways"));
        return;
    }

    QString search = m_searchEdit ? m_searchEdit->text() : QString();
    QString status = m_statusFilter ? m_statusFilter->currentText() : QString();
    QString error;

    m_currentRunways = m_service->getAllRunways(search, status, &error);

    if (!error.isEmpty() && m_currentRunways.isEmpty()) {
        m_tableWidget->setRowCount(0);
        m_stateTitle->setText(QStringLiteral("Database Unavailable"));
        m_stateMessage->setText(error);
        m_contentStack->setCurrentWidget(m_stateWidget);
        m_countBadge->setText(QStringLiteral("0 Runways"));
        m_countBadge->setStyleSheet("background-color: #fee2e2; color: #991b1b; font-size: 12px; font-weight: 600; padding: 6px 14px; border-radius: 12px; border: 1px solid #fca5a5;");
        return;
    }

    if (m_currentRunways.isEmpty()) {
        m_tableWidget->setRowCount(0);
        m_stateTitle->setText(QStringLiteral("No Runways Found"));
        m_stateMessage->setText(QStringLiteral("No runways match your search query or filter."));
        m_contentStack->setCurrentWidget(m_stateWidget);
        m_countBadge->setText(QStringLiteral("0 Runways"));
        m_countBadge->setStyleSheet("background-color: #e2e8f0; color: #334155; font-size: 12px; font-weight: 600; padding: 6px 14px; border-radius: 12px; border: 1px solid #cbd5e1;");
        return;
    }

    renderRunwayTable(m_currentRunways);
    updateDemoRunwayCombo();
    m_contentStack->setCurrentWidget(m_tableWidget);
    m_countBadge->setText(QStringLiteral("%1 Runways").arg(m_currentRunways.size()));
    m_countBadge->setStyleSheet("background-color: #e0f2fe; color: #0369a1; font-size: 12px; font-weight: 600; padding: 6px 14px; border-radius: 12px; border: 1px solid #bae6fd;");
}

void RunwaysView::renderRunwayTable(const QList<Models::Runway> &runwayList)
{
    m_tableWidget->setRowCount(0);
    m_tableWidget->setRowCount(runwayList.size());

    for (int row = 0; row < runwayList.size(); ++row) {
        const Models::Runway &rw = runwayList[row];

        // 0: ID
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(rw.id));
        idItem->setTextAlignment(Qt::AlignCenter);

        // 1: Runway Designation
        QTableWidgetItem *numItem = new QTableWidgetItem(rw.runwayNumber);
        QFont boldFont = numItem->font();
        boldFont.setBold(true);
        numItem->setFont(boldFont);
        numItem->setTextAlignment(Qt::AlignCenter);

        // 2: Permitted Operations
        QString typeLabel = rw.runwayType;
        if (rw.runwayType == QStringLiteral("Both")) {
            typeLabel = QStringLiteral("Both (Landing / Takeoff)");
        } else if (rw.runwayType == QStringLiteral("Landing")) {
            typeLabel = QStringLiteral("Landing Only");
        } else if (rw.runwayType == QStringLiteral("Takeoff")) {
            typeLabel = QStringLiteral("Takeoff Only");
        }
        QTableWidgetItem *typeItem = new QTableWidgetItem(typeLabel);
        typeItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // 3: Operational Status (Colors + Explicit text labels)
        QTableWidgetItem *statusItem = new QTableWidgetItem(rw.status);
        statusItem->setTextAlignment(Qt::AlignCenter);
        if (rw.isAvailable()) {
            statusItem->setForeground(QColor(22, 101, 52)); // Green
            statusItem->setText(QStringLiteral("● Available"));
        } else if (rw.isOccupied()) {
            statusItem->setForeground(QColor(37, 99, 235)); // Blue
            if (!rw.activeFlightNumber.isEmpty()) {
                statusItem->setText(QStringLiteral("■ Occupied (%1)").arg(rw.activeFlightNumber));
            } else {
                statusItem->setText(QStringLiteral("■ Occupied"));
            }
        } else if (rw.isMaintenance()) {
            statusItem->setForeground(QColor(180, 83, 9)); // Amber / Orange
            statusItem->setText(QStringLiteral("▲ Maintenance"));
        } else {
            statusItem->setForeground(QColor(100, 116, 139));
        }

        // 4: Occupancy / Active Allocation
        QString allocationStr;
        if (rw.isOccupied()) {
            if (!rw.activeFlightNumber.isEmpty()) {
                allocationStr = QStringLiteral("Occupied by Flight %1 (Active Assignment)").arg(rw.activeFlightNumber);
            } else {
                allocationStr = QStringLiteral("Occupied (Active Resource Hold)");
            }
        } else if (rw.isMaintenance()) {
            allocationStr = QStringLiteral("Offline for Scheduled / Safety Maintenance");
        } else {
            allocationStr = QStringLiteral("None (Free Shared Resource)");
        }

        QTableWidgetItem *allocItem = new QTableWidgetItem(allocationStr);
        allocItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        if (rw.isOccupied()) {
            allocItem->setForeground(QColor(29, 78, 216));
        } else if (rw.isMaintenance()) {
            allocItem->setForeground(QColor(146, 64, 14));
        } else {
            allocItem->setForeground(QColor(71, 85, 105));
        }

        // 5: OS Critical Section / Mutual Exclusion State
        QString csState = QStringLiteral("Resource Available");
        if (m_service && m_service->resourceManager()) {
            csState = m_service->resourceManager()->criticalSectionStatus(rw.id);
        } else if (rw.isOccupied()) {
            csState = QStringLiteral("Resource Reserved");
        } else if (rw.isMaintenance()) {
            csState = QStringLiteral("Resource Busy");
        }

        QTableWidgetItem *csItem = new QTableWidgetItem(csState);
        csItem->setTextAlignment(Qt::AlignCenter);
        if (csState == QStringLiteral("Resource Reserved")) {
            csItem->setForeground(QColor(37, 99, 235)); // Blue
        } else if (csState == QStringLiteral("Critical Section Entered")) {
            csItem->setForeground(QColor(217, 119, 6)); // Amber
        } else if (csState == QStringLiteral("Allocation Rejected") || csState == QStringLiteral("Resource Busy")) {
            csItem->setForeground(QColor(220, 38, 38)); // Red
        } else if (csState == QStringLiteral("Critical Section Released")) {
            csItem->setForeground(QColor(13, 148, 136)); // Teal
        } else {
            csItem->setForeground(QColor(22, 163, 74)); // Green (Available)
        }

        m_tableWidget->setItem(row, 0, idItem);
        m_tableWidget->setItem(row, 1, numItem);
        m_tableWidget->setItem(row, 2, typeItem);
        m_tableWidget->setItem(row, 3, statusItem);
        m_tableWidget->setItem(row, 4, allocItem);
        m_tableWidget->setItem(row, 5, csItem);
    }
}

void RunwaysView::onSearchChanged(const QString &text)
{
    Q_UNUSED(text)
    refreshRunways();
}

void RunwaysView::onStatusFilterChanged(int index)
{
    Q_UNUSED(index)
    refreshRunways();
}

void RunwaysView::onSelectionChanged()
{
    Models::Runway *rw = getSelectedRunway();
    bool hasSelection = (rw != nullptr);

    bool canMaintenance = m_service && m_service->currentSession().hasPermission(QStringLiteral("action:RunwayMaintenance"));
    bool canAllocate    = m_service && m_service->currentSession().hasPermission(QStringLiteral("action:RunwayAllocation"));
    bool canRelease     = m_service && (m_service->currentSession().hasPermission(QStringLiteral("action:RunwayAllocation")) ||
                                       m_service->currentSession().hasPermission(QStringLiteral("action:RunwayMaintenance")) ||
                                       m_service->currentSession().isAdmin());
    bool isAdmin        = m_service && m_service->currentSession().isAdmin();

    m_editButton->setEnabled(hasSelection && (isAdmin || canMaintenance));

    // Allocation button is active for ATC
    if (canAllocate) {
        if (hasSelection) {
            m_allocateButton->setEnabled(rw->isAvailable());
        } else {
            m_allocateButton->setEnabled(true);
        }
    } else {
        m_allocateButton->setEnabled(false);
    }

    // Release button is enabled when an Occupied runway is selected
    if (canRelease && hasSelection && rw->isOccupied()) {
        m_releaseButton->setEnabled(true);
    } else {
        m_releaseButton->setEnabled(false);
    }

    if (hasSelection && (canMaintenance || isAdmin)) {
        m_toggleMaintenanceBtn->setEnabled(true);
        if (rw->isMaintenance()) {
            m_toggleMaintenanceBtn->setText(QStringLiteral("✅ Return to Service"));
            m_toggleMaintenanceBtn->setStyleSheet(R"(
                QPushButton {
                    background-color: #16a34a;
                    color: #ffffff;
                    border: none;
                    border-radius: 6px;
                    padding: 8px 16px;
                    font-size: 13px;
                    font-weight: 600;
                }
                QPushButton:hover {
                    background-color: #15803d;
                }
            )");
        } else {
            m_toggleMaintenanceBtn->setText(QStringLiteral("🛠 Put in Maintenance"));
            m_toggleMaintenanceBtn->setStyleSheet(R"(
                QPushButton {
                    background-color: #d97706;
                    color: #ffffff;
                    border: none;
                    border-radius: 6px;
                    padding: 8px 16px;
                    font-size: 13px;
                    font-weight: 600;
                }
                QPushButton:hover {
                    background-color: #b45309;
                }
            )");
        }
    } else {
        m_toggleMaintenanceBtn->setEnabled(false);
        m_toggleMaintenanceBtn->setText(QStringLiteral("🛠 Set Maintenance"));
        m_toggleMaintenanceBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #cbd5e1;
                color: #94a3b8;
                border: none;
                border-radius: 6px;
                padding: 8px 16px;
                font-size: 13px;
                font-weight: 600;
            }
        )");
    }
}

Models::Runway* RunwaysView::getSelectedRunway()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_currentRunways.size()) {
        return nullptr;
    }
    return &m_currentRunways[row];
}

void RunwaysView::onToggleMaintenanceClicked()
{
    Models::Runway *rw = getSelectedRunway();
    if (!rw) {
        return;
    }

    bool targetMaintenance = !rw->isMaintenance();
    QString actionText = targetMaintenance ?
        QStringLiteral("place Runway %1 into Maintenance mode? Normal flight allocations will be suspended.").arg(rw->runwayNumber) :
        QStringLiteral("return Runway %1 to Available service?").arg(rw->runwayNumber);

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("Confirm Runway Status Change"),
        actionText,
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    QString error;
    bool ok = m_service->setRunwayMaintenanceStatus(rw->id, targetMaintenance, &error);
    if (!ok) {
        QMessageBox::critical(
            this,
            QStringLiteral("Maintenance Update Failed"),
            QStringLiteral("Could not update runway status:\n%1").arg(error)
        );
        return;
    }

    refreshRunways();
}

void RunwaysView::onAddRunwayClicked()
{
    RunwayDialog dialog(m_service, RunwayDialog::Mode::Add, this);
    if (dialog.exec() == QDialog::Accepted) {
        Models::Runway newRw = dialog.runway();
        QString error;
        bool ok = m_service->createRunway(newRw, &error);
        if (!ok) {
            QMessageBox::critical(this, QStringLiteral("Creation Failed"),
                                  QStringLiteral("Failed to create runway:\n%1").arg(error));
        } else {
            refreshRunways();
        }
    }
}

void RunwaysView::onEditRunwayClicked()
{
    Models::Runway *rw = getSelectedRunway();
    if (!rw) {
        return;
    }

    RunwayDialog dialog(m_service, RunwayDialog::Mode::Edit, this);
    dialog.setRunway(*rw);
    if (dialog.exec() == QDialog::Accepted) {
        Models::Runway updatedRw = dialog.runway();
        QString error;
        bool ok = m_service->updateRunway(updatedRw, &error);
        if (!ok) {
            QMessageBox::critical(this, QStringLiteral("Update Failed"),
                                  QStringLiteral("Failed to update runway:\n%1").arg(error));
        } else {
            refreshRunways();
        }
    }
}

void RunwaysView::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(row)
    Q_UNUSED(column)
    onEditRunwayClicked();
}

void RunwaysView::onAllocateClicked()
{
    Models::Runway *rw = getSelectedRunway();
    int preselectedId = (rw && rw->isAvailable()) ? rw->id : 0;
    RunwayAllocationDialog dialog(m_service, preselectedId, this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshRunways();
    }
}

void RunwaysView::onReleaseClicked()
{
    Models::Runway *rw = getSelectedRunway();
    if (!rw) {
        return;
    }

    if (!rw->isOccupied()) {
        QMessageBox::information(
            this,
            QStringLiteral("Release Runway"),
            QStringLiteral("Runway %1 is not currently Occupied.").arg(rw->runwayNumber)
        );
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("Release Runway Resource"),
        QStringLiteral("Are you sure you want to release Runway %1 from its active flight allocation?\n"
                       "The runway status will return to Available.")
            .arg(rw->runwayNumber),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    QString error;
    bool ok = m_service->releaseRunway(rw->id, &error);
    if (!ok) {
        QMessageBox::warning(
            this,
            QStringLiteral("Release Failed"),
            QStringLiteral("Failed to release runway %1:\n%2").arg(rw->runwayNumber, error)
        );
        return;
    }

    refreshRunways();
}

// ─────────────────────────────────────────────────────────────────────────────
// Concurrent Allocation Demonstration Slots & Helpers (Day 20)
// ─────────────────────────────────────────────────────────────────────────────

void RunwaysView::updateDemoRunwayCombo()
{
    if (!m_demoRunwayCombo) {
        return;
    }

    int prevSelectedId = m_demoRunwayCombo->currentData().toInt();
    m_demoRunwayCombo->clear();

    for (const auto &rw : m_currentRunways) {
        QString text = QStringLiteral("Runway %1 (%2)").arg(rw.runwayNumber, rw.status);
        m_demoRunwayCombo->addItem(text, rw.id);
    }

    if (prevSelectedId > 0) {
        int idx = m_demoRunwayCombo->findData(prevSelectedId);
        if (idx >= 0) {
            m_demoRunwayCombo->setCurrentIndex(idx);
        }
    }
}

void RunwaysView::onStartConcurrentTestClicked()
{
    if (!m_demoEngine) {
        return;
    }

    if (!m_demoRunwayCombo || m_demoRunwayCombo->count() == 0) {
        QMessageBox::warning(this, QStringLiteral("Concurrent Test"), QStringLiteral("No runway available. Please ensure runways exist and are loaded."));
        return;
    }

    int runwayId = m_demoRunwayCombo->currentData().toInt();
    if (runwayId <= 0) {
        QMessageBox::warning(this, QStringLiteral("Concurrent Test"), QStringLiteral("Invalid runway selected."));
        return;
    }

    int threadCount = m_demoThreadSpin ? m_demoThreadSpin->value() : 3;

    if (m_startDemoBtn) {
        m_startDemoBtn->setEnabled(false);
    }

    m_demoEngine->startAsyncDefault(runwayId, threadCount);
}

void RunwaysView::onResetDemoClicked()
{
    if (m_demoEngine) {
        m_demoEngine->cancelAndJoin();
    }

    if (m_demoTable) {
        m_demoTable->setRowCount(0);
    }

    if (m_totalReqCard) m_totalReqCard->setText(QStringLiteral("0 Requests"));
    if (m_successCard) m_successCard->setText(QStringLiteral("0 Winner"));
    if (m_rejectedCard) m_rejectedCard->setText(QStringLiteral("0 Conflicts"));
    if (m_winnerCard) m_winnerCard->setText(QStringLiteral("None (Available)"));

    if (m_demoStatusLabel) {
        m_demoStatusLabel->setText(QStringLiteral("ℹ️ Concurrency demonstration reset. Ready to launch test."));
        m_demoStatusLabel->setStyleSheet(R"(
            QLabel {
                background-color: #f8fafc;
                color: #334155;
                border: 1px solid #e2e8f0;
                border-radius: 6px;
                padding: 10px 14px;
                font-size: 13px;
                font-weight: 500;
            }
        )");
    }

    updatePermissions();
}

void RunwaysView::onRefreshResourceStateClicked()
{
    refreshRunways();
    if (m_demoStatusLabel) {
        m_demoStatusLabel->setText(QStringLiteral("🔄 Shared resource states synchronized with MySQL and in-memory ResourceManager."));
    }
}

void RunwaysView::onDemoStarted(int runwayId, int totalThreads)
{
    if (m_demoStatusLabel) {
        m_demoStatusLabel->setText(QStringLiteral("🚀 [Runway %1] %2 worker threads spawned and waiting at start barrier condition variable...")
            .arg(runwayId).arg(totalThreads));
        m_demoStatusLabel->setStyleSheet(R"(
            QLabel {
                background-color: #fefce8;
                color: #854d0e;
                border: 1px solid #fef08a;
                border-radius: 6px;
                padding: 10px 14px;
                font-size: 13px;
                font-weight: 500;
            }
        )");
    }

    if (m_demoTable) {
        m_demoTable->setRowCount(totalThreads);
        auto reqs = ConcurrentAllocationDemo::createDefaultRequests(runwayId, totalThreads);

        for (int i = 0; i < totalThreads; ++i) {
            auto itemLabel = new QTableWidgetItem(reqs[i].threadLabel);
            auto itemSysId = new QTableWidgetItem(QStringLiteral("Spawning..."));
            auto itemFlight = new QTableWidgetItem(QStringLiteral("%1 (#%2)").arg(reqs[i].flightNumber).arg(reqs[i].flightId));
            auto itemRw = new QTableWidgetItem(QStringLiteral("Runway %1").arg(runwayId));
            auto itemStatus = new QTableWidgetItem(QStringLiteral("Waiting to Start"));
            auto itemResult = new QTableWidgetItem(QStringLiteral("PENDING"));
            auto itemMsg = new QTableWidgetItem(QStringLiteral("Waiting at std::condition_variable start barrier..."));

            itemLabel->setTextAlignment(Qt::AlignCenter);
            itemSysId->setTextAlignment(Qt::AlignCenter);
            itemFlight->setTextAlignment(Qt::AlignCenter);
            itemRw->setTextAlignment(Qt::AlignCenter);
            itemStatus->setTextAlignment(Qt::AlignCenter);
            itemResult->setTextAlignment(Qt::AlignCenter);

            itemStatus->setForeground(QColor(100, 116, 139));
            itemResult->setForeground(QColor(100, 116, 139));

            m_demoTable->setItem(i, 0, itemLabel);
            m_demoTable->setItem(i, 1, itemSysId);
            m_demoTable->setItem(i, 2, itemFlight);
            m_demoTable->setItem(i, 3, itemRw);
            m_demoTable->setItem(i, 4, itemStatus);
            m_demoTable->setItem(i, 5, itemResult);
            m_demoTable->setItem(i, 6, itemMsg);
        }
    }
}

void RunwaysView::onThreadStateChanged(const ConcurrentAllocationDemo::ThreadResult &result)
{
    if (!m_demoTable) {
        return;
    }

    int row = result.requestId - 1;
    if (row < 0 || row >= m_demoTable->rowCount()) {
        return;
    }

    if (m_demoTable->item(row, 1)) {
        m_demoTable->item(row, 1)->setText(result.systemThreadId);
    }
    if (m_demoTable->item(row, 4)) {
        m_demoTable->item(row, 4)->setText(result.status);
    }
    if (m_demoTable->item(row, 5)) {
        if (result.success) {
            m_demoTable->item(row, 5)->setText(QStringLiteral("SUCCESS"));
            m_demoTable->item(row, 5)->setForeground(QColor(22, 163, 74)); // Green
        } else if (result.status == QStringLiteral("Allocation Rejected") || result.status == QStringLiteral("Thread Failed")) {
            m_demoTable->item(row, 5)->setText(QStringLiteral("CONFLICT"));
            m_demoTable->item(row, 5)->setForeground(QColor(220, 38, 38)); // Red
        } else {
            m_demoTable->item(row, 5)->setText(QStringLiteral("IN PROGRESS"));
            m_demoTable->item(row, 5)->setForeground(QColor(217, 119, 6)); // Amber
        }
    }
    if (m_demoTable->item(row, 6)) {
        m_demoTable->item(row, 6)->setText(result.message);
    }
}

void RunwaysView::onDemoFinished(const ConcurrentAllocationDemo::DemoReport &report)
{
    updatePermissions();

    if (m_totalReqCard) {
        m_totalReqCard->setText(QStringLiteral("%1 Requests").arg(report.totalRequests));
    }
    if (m_successCard) {
        m_successCard->setText(QStringLiteral("%1 Winner").arg(report.successfulAllocations));
    }
    if (m_rejectedCard) {
        m_rejectedCard->setText(QStringLiteral("%1 Conflicts").arg(report.rejectedAllocations));
    }
    if (m_winnerCard) {
        m_winnerCard->setText(report.finalOwningFlightNumber);
    }

    if (m_demoStatusLabel) {
        m_demoStatusLabel->setText(report.summaryMessage);
        if (!report.hasDataRace && report.successfulAllocations <= 1) {
            m_demoStatusLabel->setStyleSheet(R"(
                QLabel {
                    background-color: #f0fdf4;
                    color: #166534;
                    border: 1px solid #bbf7d0;
                    border-radius: 6px;
                    padding: 10px 14px;
                    font-size: 13px;
                    font-weight: 600;
                }
            )");
        } else {
            m_demoStatusLabel->setStyleSheet(R"(
                QLabel {
                    background-color: #fef2f2;
                    color: #991b1b;
                    border: 1px solid #fecaca;
                    border-radius: 6px;
                    padding: 10px 14px;
                    font-size: 13px;
                    font-weight: 600;
                }
            )");
        }
    }

    refreshRunways();
}
