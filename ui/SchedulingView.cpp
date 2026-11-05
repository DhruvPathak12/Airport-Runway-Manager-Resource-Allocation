#include "ui/SchedulingView.h"
#include "services/ApplicationService.h"
#include "scheduler/Scheduler.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QFont>
#include <QGroupBox>

SchedulingView::SchedulingView(ApplicationService *service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
    , m_clockTimer(new QTimer(this))
{
    setupUi();

    if (m_service) {
        connect(m_service, &ApplicationService::processesChanged, this, &SchedulingView::refreshView);
        connect(m_service, &ApplicationService::flightsChanged, this, &SchedulingView::refreshView);
        if (m_service->scheduler()) {
            connect(m_service->scheduler(), &Scheduler::schedulingComplete, this, &SchedulingView::refreshView);
            connect(m_service->scheduler(), &Scheduler::processDispatched, this, &SchedulingView::refreshView);
            connect(m_service->scheduler(), &Scheduler::processCompleted, this, &SchedulingView::refreshView);
        }
    }

    // Live clock timer for updating continuous wait durations smoothly
    connect(m_clockTimer, &QTimer::timeout, this, &SchedulingView::onClockTick);
    m_clockTimer->start(1000);

    refreshView();
    updatePermissions();
    onResetSimulationClicked();
}

SchedulingView::~SchedulingView()
{
    if (m_clockTimer) {
        m_clockTimer->stop();
    }
}

void SchedulingView::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
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

    m_tabWidget->addTab(createLiveTab(), QStringLiteral("⏱ Live Scheduler & Ready Queue"));
    m_tabWidget->addTab(createStarvationTab(), QStringLiteral("🔬 Starvation Demonstration (OS Verification)"));

    mainLayout->addWidget(m_tabWidget);
}

QWidget* SchedulingView::createLiveTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // ── 1. Header ────────────────────────────────────────────────────────────
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QLabel *titleLabel = new QLabel(QStringLiteral("⏱ Priority Scheduling & Aging Monitor"), tab);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(15);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet(QStringLiteral("color: #0f172a;"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_statusBadge = new QLabel(QStringLiteral("⚪ Aging: Stopped"), tab);
    m_statusBadge->setStyleSheet(R"(
        QLabel {
            background-color: #f1f5f9;
            color: #475569;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 12px;
            font-weight: 600;
        }
    )");
    headerLayout->addWidget(m_statusBadge);
    layout->addLayout(headerLayout);

    // ── 2. Toolbar ───────────────────────────────────────────────────────────
    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    toolbarLayout->setSpacing(8);

    QLabel *intervalLabel = new QLabel(QStringLiteral("Interval:"), tab);
    intervalLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: #334155; font-size: 13px;"));
    toolbarLayout->addWidget(intervalLabel);

    m_intervalSpin = new QSpinBox(tab);
    m_intervalSpin->setRange(1, 3600);
    m_intervalSpin->setValue(m_service ? m_service->agingInterval() : 30);
    m_intervalSpin->setSuffix(QStringLiteral(" s"));
    m_intervalSpin->setFixedWidth(90);
    m_intervalSpin->setStyleSheet(R"(
        QSpinBox {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 6px 8px;
            font-size: 13px;
        }
    )");
    connect(m_intervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SchedulingView::onIntervalChanged);
    toolbarLayout->addWidget(m_intervalSpin);

    m_startAgingButton = new QPushButton(QStringLiteral("▶ Start Aging"), tab);
    m_startAgingButton->setStyleSheet(R"(
        QPushButton {
            background-color: #16a34a;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 600;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #15803d; }
        QPushButton:disabled { background-color: #94a3b8; color: #f1f5f9; }
    )");
    connect(m_startAgingButton, &QPushButton::clicked, this, &SchedulingView::onStartAgingClicked);
    toolbarLayout->addWidget(m_startAgingButton);

    m_stopAgingButton = new QPushButton(QStringLiteral("⏹ Stop Aging"), tab);
    m_stopAgingButton->setStyleSheet(R"(
        QPushButton {
            background-color: #dc2626;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 600;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #b91c1c; }
        QPushButton:disabled { background-color: #94a3b8; color: #f1f5f9; }
    )");
    connect(m_stopAgingButton, &QPushButton::clicked, this, &SchedulingView::onStopAgingClicked);
    toolbarLayout->addWidget(m_stopAgingButton);

    m_evalAgingButton = new QPushButton(QStringLiteral("⚡ Evaluate Aging Now"), tab);
    m_evalAgingButton->setStyleSheet(R"(
        QPushButton {
            background-color: #0284c7;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 600;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #0369a1; }
        QPushButton:disabled { background-color: #94a3b8; color: #f1f5f9; }
    )");
    connect(m_evalAgingButton, &QPushButton::clicked, this, &SchedulingView::onEvaluateAgingClicked);
    toolbarLayout->addWidget(m_evalAgingButton);

    m_dispatchButton = new QPushButton(QStringLiteral("🚀 Dispatch Next Process"), tab);
    m_dispatchButton->setStyleSheet(R"(
        QPushButton {
            background-color: #4f46e5;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 600;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #4338ca; }
        QPushButton:disabled { background-color: #94a3b8; color: #f1f5f9; }
    )");
    connect(m_dispatchButton, &QPushButton::clicked, this, &SchedulingView::onDispatchClicked);
    toolbarLayout->addWidget(m_dispatchButton);

    toolbarLayout->addStretch();

    m_refreshButton = new QPushButton(QStringLiteral("🔄 Refresh"), tab);
    m_refreshButton->setStyleSheet(R"(
        QPushButton {
            background-color: #f1f5f9;
            color: #334155;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 6px 12px;
            font-weight: 600;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #e2e8f0; }
    )");
    connect(m_refreshButton, &QPushButton::clicked, this, &SchedulingView::refreshView);
    toolbarLayout->addWidget(m_refreshButton);

    layout->addLayout(toolbarLayout);

    // ── RBAC Notice ──────────────────────────────────────────────────────────
    m_permNotice = new QLabel(tab);
    m_permNotice->setStyleSheet(R"(
        QLabel {
            background-color: #fffbeb;
            color: #92400e;
            border: 1px solid #fde68a;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
        }
    )");
    m_permNotice->setVisible(false);
    layout->addWidget(m_permNotice);

    // ── Running Process Banner ───────────────────────────────────────────────
    m_runningBanner = new QLabel(QStringLiteral("CPU / Runway Dispatcher: IDLE (No process running)"), tab);
    m_runningBanner->setStyleSheet(R"(
        QLabel {
            background-color: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            padding: 8px 12px;
            font-size: 13px;
            font-weight: 600;
            color: #1e293b;
        }
    )");
    layout->addWidget(m_runningBanner);

    // ── Processes Table ──────────────────────────────────────────────────────
    m_tableWidget = new QTableWidget(tab);
    m_tableWidget->setColumnCount(10);
    m_tableWidget->setHorizontalHeaderLabels({
        QStringLiteral("PID (Flight ID)"),
        QStringLiteral("Flight #"),
        QStringLiteral("Airline"),
        QStringLiteral("Base Priority"),
        QStringLiteral("Effective Priority"),
        QStringLiteral("Emergency"),
        QStringLiteral("Aging Level"),
        QStringLiteral("Continuous Wait"),
        QStringLiteral("Process State"),
        QStringLiteral("Burst Time")
    });

    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setShowGrid(false);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    for (int col = 0; col < 9; ++col) {
        m_tableWidget->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    }

    m_tableWidget->setStyleSheet(R"(
        QTableWidget {
            background-color: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            gridline-color: #f1f5f9;
            font-size: 13px;
        }
        QTableWidget::item {
            padding: 6px 10px;
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
            padding: 8px 10px;
            border: none;
            border-bottom: 2px solid #e2e8f0;
        }
    )");

    layout->addWidget(m_tableWidget);
    return tab;
}

QWidget* SchedulingView::createStarvationTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // ── 1. Header ────────────────────────────────────────────────────────────
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QLabel *titleLabel = new QLabel(QStringLiteral("🔬 Starvation & Priority Aging Verification"), tab);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(15);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet(QStringLiteral("color: #0f172a;"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_simClockLabel = new QLabel(QStringLiteral("Simulated Time: 0s"), tab);
    m_simClockLabel->setStyleSheet(R"(
        QLabel {
            background-color: #f1f5f9;
            color: #0f172a;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 12px;
            font-weight: 600;
        }
    )");
    headerLayout->addWidget(m_simClockLabel);

    m_simStatusBadge = new QLabel(QStringLiteral("⚪ Idle"), tab);
    m_simStatusBadge->setStyleSheet(R"(
        QLabel {
            background-color: #f1f5f9;
            color: #475569;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 12px;
            font-weight: 600;
        }
    )");
    headerLayout->addWidget(m_simStatusBadge);
    layout->addLayout(headerLayout);

    // ── 2. Toolbar & Controls ────────────────────────────────────────────────
    QHBoxLayout *toolbar = new QHBoxLayout;
    toolbar->setSpacing(8);

    m_btnRunWithoutAging = new QPushButton(QStringLiteral("▶ Run WITHOUT Aging (Mode A)"), tab);
    m_btnRunWithoutAging->setStyleSheet(R"(
        QPushButton {
            background-color: #e11d48;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 7px 14px;
            font-weight: 600;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #be123c; }
        QPushButton:disabled { background-color: #94a3b8; color: #f1f5f9; }
    )");
    connect(m_btnRunWithoutAging, &QPushButton::clicked, this, &SchedulingView::onRunWithoutAgingClicked);
    toolbar->addWidget(m_btnRunWithoutAging);

    m_btnRunWithAging = new QPushButton(QStringLiteral("⚡ Run WITH Aging (Mode B)"), tab);
    m_btnRunWithAging->setStyleSheet(R"(
        QPushButton {
            background-color: #16a34a;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 7px 14px;
            font-weight: 600;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #15803d; }
        QPushButton:disabled { background-color: #94a3b8; color: #f1f5f9; }
    )");
    connect(m_btnRunWithAging, &QPushButton::clicked, this, &SchedulingView::onRunWithAgingClicked);
    toolbar->addWidget(m_btnRunWithAging);

    m_btnStepSim = new QPushButton(QStringLiteral("⏭ Step"), tab);
    m_btnStepSim->setStyleSheet(R"(
        QPushButton {
            background-color: #0284c7;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 7px 12px;
            font-weight: 600;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #0369a1; }
        QPushButton:disabled { background-color: #94a3b8; color: #f1f5f9; }
    )");
    connect(m_btnStepSim, &QPushButton::clicked, this, &SchedulingView::onStepSimulationClicked);
    toolbar->addWidget(m_btnStepSim);

    m_btnResetSim = new QPushButton(QStringLiteral("🔄 Reset"), tab);
    m_btnResetSim->setStyleSheet(R"(
        QPushButton {
            background-color: #f1f5f9;
            color: #334155;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 7px 12px;
            font-weight: 600;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #e2e8f0; }
        QPushButton:disabled { background-color: #94a3b8; color: #f1f5f9; }
    )");
    connect(m_btnResetSim, &QPushButton::clicked, this, &SchedulingView::onResetSimulationClicked);
    toolbar->addWidget(m_btnResetSim);

    toolbar->addSpacing(10);
    QLabel *agingLbl = new QLabel(QStringLiteral("Aging Interval:"), tab);
    agingLbl->setStyleSheet(QStringLiteral("font-weight: 600; color: #334155; font-size: 12px;"));
    toolbar->addWidget(agingLbl);

    m_simAgingIntervalSpin = new QSpinBox(tab);
    m_simAgingIntervalSpin->setRange(5, 300);
    m_simAgingIntervalSpin->setValue(30);
    m_simAgingIntervalSpin->setSuffix(QStringLiteral(" s"));
    m_simAgingIntervalSpin->setFixedWidth(80);
    m_simAgingIntervalSpin->setStyleSheet(R"(
        QSpinBox {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 5px 6px;
            font-size: 12px;
        }
    )");
    toolbar->addWidget(m_simAgingIntervalSpin);

    QLabel *horizLbl = new QLabel(QStringLiteral("Horizon:"), tab);
    horizLbl->setStyleSheet(QStringLiteral("font-weight: 600; color: #334155; font-size: 12px;"));
    toolbar->addWidget(horizLbl);

    m_simHorizonSpin = new QSpinBox(tab);
    m_simHorizonSpin->setRange(30, 600);
    m_simHorizonSpin->setValue(180);
    m_simHorizonSpin->setSuffix(QStringLiteral(" s"));
    m_simHorizonSpin->setFixedWidth(85);
    m_simHorizonSpin->setStyleSheet(R"(
        QSpinBox {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 5px 6px;
            font-size: 12px;
        }
    )");
    toolbar->addWidget(m_simHorizonSpin);

    toolbar->addStretch();
    layout->addLayout(toolbar);

    // ── RBAC Notice ──────────────────────────────────────────────────────────
    m_simPermNotice = new QLabel(tab);
    m_simPermNotice->setStyleSheet(R"(
        QLabel {
            background-color: #fffbeb;
            color: #92400e;
            border: 1px solid #fde68a;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
        }
    )");
    m_simPermNotice->setVisible(false);
    layout->addWidget(m_simPermNotice);

    // ── 3. Side-by-Side Comparison Summary Table ─────────────────────────────
    QLabel *compTitle = new QLabel(QStringLiteral("📊 Side-by-Side Verification: Without Aging vs With Aging"), tab);
    compTitle->setStyleSheet(QStringLiteral("font-weight: 700; color: #1e293b; font-size: 13px;"));
    layout->addWidget(compTitle);

    m_comparisonTable = new QTableWidget(tab);
    m_comparisonTable->setColumnCount(3);
    m_comparisonTable->setHorizontalHeaderLabels({
        QStringLiteral("Scheduling Metric"),
        QStringLiteral("Mode A: Priority Scheduling WITHOUT Aging"),
        QStringLiteral("Mode B: Priority Scheduling WITH Aging")
    });
    m_comparisonTable->setRowCount(7);
    m_comparisonTable->setVerticalHeaderLabels({
        QStringLiteral("Target Process"),
        QStringLiteral("Base Priority"),
        QStringLiteral("Final Effective Priority"),
        QStringLiteral("READY Waiting Duration"),
        QStringLiteral("Higher-Priority Dispatches Before It"),
        QStringLiteral("Dispatched Within Horizon"),
        QStringLiteral("Starvation Outcome")
    });

    m_comparisonTable->setItem(0, 0, new QTableWidgetItem(QStringLiteral("AI-101 (PID 101)")));
    m_comparisonTable->setItem(1, 0, new QTableWidgetItem(QStringLiteral("5 (Lowest)")));
    m_comparisonTable->setItem(2, 0, new QTableWidgetItem(QStringLiteral("Effective Priority")));
    m_comparisonTable->setItem(3, 0, new QTableWidgetItem(QStringLiteral("Waiting Time (s)")));
    m_comparisonTable->setItem(4, 0, new QTableWidgetItem(QStringLiteral("Dispatches Prior")));
    m_comparisonTable->setItem(5, 0, new QTableWidgetItem(QStringLiteral("Dispatch Status")));
    m_comparisonTable->setItem(6, 0, new QTableWidgetItem(QStringLiteral("Starvation Status")));

    m_comparisonTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_comparisonTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_comparisonTable->horizontalHeader()->setStretchLastSection(true);
    m_comparisonTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_comparisonTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_comparisonTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_comparisonTable->setFixedHeight(225);

    m_comparisonTable->setStyleSheet(R"(
        QTableWidget {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            font-size: 12px;
        }
        QTableWidget::item {
            padding: 5px 10px;
            border-bottom: 1px solid #f1f5f9;
        }
        QHeaderView::section {
            background-color: #f8fafc;
            color: #334155;
            font-weight: 700;
            padding: 6px 10px;
            border: none;
            border-bottom: 2px solid #cbd5e1;
        }
    )");
    layout->addWidget(m_comparisonTable);

    // ── 4. Educational Explanation Box ───────────────────────────────────────
    m_explanationBox = new QLabel(tab);
    m_explanationBox->setWordWrap(true);
    m_explanationBox->setStyleSheet(R"(
        QLabel {
            background-color: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            padding: 10px 14px;
            font-size: 12px;
            color: #334155;
            line-height: 1.4;
        }
    )");
    m_explanationBox->setText(QStringLiteral(
        "<b>Operating System Starvation & Aging Concept:</b><br/>"
        "• <b>Without Aging (Mode A):</b> Low-priority flight AI-101 (base priority 5) starves indefinitely in READY state because incoming higher-priority flights (priorities 1 and 2) continuously monopolize the non-preemptive dispatcher.<br/>"
        "• <b>With Aging (Mode B):</b> For each 30s spent continuously waiting in READY, AI-101's effective priority improves by one level (5 → 4 → 3 → 2 → 1). Upon reaching priority 1, deterministic tie-breaking (earliest arrival at t=0s) grants it CPU scheduling preference, proving starvation prevention."
    ));
    layout->addWidget(m_explanationBox);

    // ── 5. Detailed Process Simulation Table ──────────────────────────────────
    QLabel *tableTitle = new QLabel(QStringLiteral("📋 Active Simulation Processes & Dispatch Timeline"), tab);
    tableTitle->setStyleSheet(QStringLiteral("font-weight: 700; color: #1e293b; font-size: 13px;"));
    layout->addWidget(tableTitle);

    m_simProcessesTable = new QTableWidget(tab);
    m_simProcessesTable->setColumnCount(10);
    m_simProcessesTable->setHorizontalHeaderLabels({
        QStringLiteral("PID"),
        QStringLiteral("Flight #"),
        QStringLiteral("Airline"),
        QStringLiteral("Base Pri"),
        QStringLiteral("Eff Pri"),
        QStringLiteral("Aging Lvl"),
        QStringLiteral("Arrival (s)"),
        QStringLiteral("Wait Time (s)"),
        QStringLiteral("State"),
        QStringLiteral("Dispatch Time")
    });

    m_simProcessesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_simProcessesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_simProcessesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_simProcessesTable->setAlternatingRowColors(true);
    m_simProcessesTable->setShowGrid(false);
    m_simProcessesTable->verticalHeader()->setVisible(false);
    m_simProcessesTable->horizontalHeader()->setStretchLastSection(true);
    for (int col = 0; col < 9; ++col) {
        m_simProcessesTable->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    }

    m_simProcessesTable->setStyleSheet(R"(
        QTableWidget {
            background-color: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            font-size: 12px;
        }
        QTableWidget::item {
            padding: 5px 8px;
            color: #1e293b;
            border-bottom: 1px solid #f1f5f9;
        }
        QHeaderView::section {
            background-color: #f8fafc;
            color: #475569;
            font-weight: 600;
            padding: 6px 8px;
            border: none;
            border-bottom: 2px solid #e2e8f0;
        }
    )");
    layout->addWidget(m_simProcessesTable);

    return tab;
}

void SchedulingView::updatePermissions()
{
    if (!m_service) {
        return;
    }

    bool canManage = m_service->currentSession().hasPermission(QStringLiteral("action:ManageScheduling"));

    // Live Tab controls
    m_startAgingButton->setEnabled(canManage && !m_service->isAgingRunning());
    m_stopAgingButton->setEnabled(canManage && m_service->isAgingRunning());
    m_intervalSpin->setEnabled(canManage);
    m_evalAgingButton->setEnabled(canManage);
    m_dispatchButton->setEnabled(canManage);

    // Starvation Tab controls
    m_btnRunWithoutAging->setEnabled(canManage);
    m_btnRunWithAging->setEnabled(canManage);
    m_btnStepSim->setEnabled(canManage);
    m_btnResetSim->setEnabled(canManage);
    m_simAgingIntervalSpin->setEnabled(canManage);
    m_simHorizonSpin->setEnabled(canManage);

    if (!canManage) {
        QString notice = QStringLiteral("Notice: Role '%1' possesses view-only access. Priority scheduling and starvation demonstration controls are restricted to ATC and Administrator roles.")
                             .arg(m_service->currentSession().role());
        m_permNotice->setText(notice);
        m_permNotice->setVisible(true);

        m_simPermNotice->setText(notice);
        m_simPermNotice->setVisible(true);
    } else {
        m_permNotice->setVisible(false);
        m_simPermNotice->setVisible(false);
    }

    updateStatusBanner();
}

void SchedulingView::updateStatusBanner()
{
    if (!m_service) {
        return;
    }

    bool running = m_service->isAgingRunning();
    int interval = m_service->agingInterval();

    if (running) {
        m_statusBadge->setText(QStringLiteral("🟢 Aging: Running (%1s)").arg(interval));
        m_statusBadge->setStyleSheet(R"(
            QLabel {
                background-color: #dcfce7;
                color: #15803d;
                border: 1px solid #86efac;
                border-radius: 6px;
                padding: 6px 12px;
                font-size: 12px;
                font-weight: 600;
            }
        )");
    } else {
        m_statusBadge->setText(QStringLiteral("⚪ Aging: Stopped (%1s)").arg(interval));
        m_statusBadge->setStyleSheet(R"(
            QLabel {
                background-color: #f1f5f9;
                color: #475569;
                border: 1px solid #cbd5e1;
                border-radius: 6px;
                padding: 6px 12px;
                font-size: 12px;
                font-weight: 600;
            }
        )");
    }

    if (m_service->scheduler() && m_service->scheduler()->hasRunningProcess()) {
        Models::Process runningProc = m_service->scheduler()->runningProcess();
        m_runningBanner->setText(QStringLiteral("CPU / Runway Dispatcher: RUNNING (Flight %1 [%2], Priority %3)")
                                     .arg(runningProc.flightNumber.isEmpty() ? QString::number(runningProc.flightId) : runningProc.flightNumber)
                                     .arg(runningProc.airline.isEmpty() ? QStringLiteral("Airline") : runningProc.airline)
                                     .arg(runningProc.effectivePriority));
        m_runningBanner->setStyleSheet(R"(
            QLabel {
                background-color: #dbeafe;
                border: 1px solid #93c5fd;
                border-radius: 8px;
                padding: 8px 12px;
                font-size: 13px;
                font-weight: 600;
                color: #1e40af;
            }
        )");
    } else {
        m_runningBanner->setText(QStringLiteral("CPU / Runway Dispatcher: IDLE (No process running)"));
        m_runningBanner->setStyleSheet(R"(
            QLabel {
                background-color: #f8fafc;
                border: 1px solid #e2e8f0;
                border-radius: 8px;
                padding: 8px 12px;
                font-size: 13px;
                font-weight: 600;
                color: #475569;
            }
        )");
    }
}

void SchedulingView::refreshView()
{
    if (!m_service) {
        return;
    }

    QList<Models::Process> queueProcs;
    if (m_service->readyQueue()) {
        queueProcs = m_service->readyQueue()->allProcesses();
    }

    m_cachedProcesses = queueProcs;
    renderProcessesTable(m_cachedProcesses);
    updateStatusBanner();
}

void SchedulingView::renderProcessesTable(const QList<Models::Process> &processes)
{
    m_tableWidget->setRowCount(processes.size());
    QDateTime now = QDateTime::currentDateTime();

    for (int row = 0; row < processes.size(); ++row) {
        const Models::Process &p = processes.at(row);

        // Col 0: PID
        QTableWidgetItem *pidItem = new QTableWidgetItem(QString::number(p.flightId));
        pidItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(row, 0, pidItem);

        // Col 1: Flight #
        QTableWidgetItem *fnItem = new QTableWidgetItem(p.flightNumber.isEmpty() ? QStringLiteral("—") : p.flightNumber);
        QFont boldFont;
        boldFont.setBold(true);
        fnItem->setFont(boldFont);
        m_tableWidget->setItem(row, 1, fnItem);

        // Col 2: Airline
        m_tableWidget->setItem(row, 2, new QTableWidgetItem(p.airline.isEmpty() ? QStringLiteral("—") : p.airline));

        // Col 3: Base Priority
        QTableWidgetItem *baseItem = new QTableWidgetItem(QStringLiteral("%1 - %2").arg(p.basePriority).arg(
            p.basePriority == 1 ? QStringLiteral("Highest") :
            p.basePriority == 2 ? QStringLiteral("High") :
            p.basePriority == 3 ? QStringLiteral("Normal") :
            p.basePriority == 4 ? QStringLiteral("Low") : QStringLiteral("Lowest")));
        baseItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(row, 3, baseItem);

        // Col 4: Effective Priority
        QString effText;
        if (p.isEmergency) {
            effText = QStringLiteral("1 - 🚨 EMERGENCY");
        } else {
            effText = QStringLiteral("%1 - %2").arg(p.effectivePriority).arg(
                p.effectivePriority == 1 ? QStringLiteral("Highest") :
                p.effectivePriority == 2 ? QStringLiteral("High") :
                p.effectivePriority == 3 ? QStringLiteral("Normal") :
                p.effectivePriority == 4 ? QStringLiteral("Low") : QStringLiteral("Lowest"));
        }
        QTableWidgetItem *effItem = new QTableWidgetItem(effText);
        effItem->setTextAlignment(Qt::AlignCenter);
        if (p.isEmergency) {
            effItem->setForeground(QBrush(QColor(185, 28, 28)));
            effItem->setFont(boldFont);
        } else if (p.effectivePriority < p.basePriority) {
            effItem->setForeground(QBrush(QColor(21, 128, 61)));
            effItem->setFont(boldFont);
        }
        m_tableWidget->setItem(row, 4, effItem);

        // Col 5: Emergency
        QTableWidgetItem *emItem = new QTableWidgetItem(p.isEmergency ? QStringLiteral("🚨 YES") : QStringLiteral("No"));
        emItem->setTextAlignment(Qt::AlignCenter);
        if (p.isEmergency) {
            emItem->setForeground(QBrush(QColor(220, 38, 38)));
            emItem->setFont(boldFont);
        }
        m_tableWidget->setItem(row, 5, emItem);

        // Col 6: Aging Level
        QTableWidgetItem *agingItem = new QTableWidgetItem(QString::number(p.agingLevel));
        agingItem->setTextAlignment(Qt::AlignCenter);
        if (p.agingLevel > 0) {
            agingItem->setForeground(QBrush(QColor(21, 128, 61)));
            agingItem->setFont(boldFont);
        }
        m_tableWidget->setItem(row, 6, agingItem);

        // Col 7: Continuous Wait
        int waitSec = p.readyWaitingDurationSeconds(now);
        QString waitText = (waitSec >= 60) ? QStringLiteral("%1m %2s").arg(waitSec / 60).arg(waitSec % 60)
                                           : QStringLiteral("%1s").arg(waitSec);
        QTableWidgetItem *waitItem = new QTableWidgetItem(waitText);
        waitItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(row, 7, waitItem);

        // Col 8: State
        QTableWidgetItem *stItem = new QTableWidgetItem(Models::Process::stateToString(p.state));
        stItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(row, 8, stItem);

        // Col 9: Burst Time
        QTableWidgetItem *burstItem = new QTableWidgetItem(QStringLiteral("%1s").arg(p.serviceTime));
        burstItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(row, 9, burstItem);
    }
}

void SchedulingView::onClockTick()
{
    QDateTime now = QDateTime::currentDateTime();
    for (int row = 0; row < m_cachedProcesses.size() && row < m_tableWidget->rowCount(); ++row) {
        const Models::Process &p = m_cachedProcesses.at(row);
        int waitSec = p.readyWaitingDurationSeconds(now);
        QString waitText = (waitSec >= 60) ? QStringLiteral("%1m %2s").arg(waitSec / 60).arg(waitSec % 60)
                                           : QStringLiteral("%1s").arg(waitSec);
        QTableWidgetItem *item = m_tableWidget->item(row, 7);
        if (item) {
            item->setText(waitText);
        }
    }
}

void SchedulingView::onStartAgingClicked()
{
    if (!m_service) return;
    QString err;
    if (!m_service->startAging(&err)) {
        QMessageBox::warning(this, QStringLiteral("Start Aging Failed"), err);
    } else {
        updateStatusBanner();
    }
}

void SchedulingView::onStopAgingClicked()
{
    if (!m_service) return;
    QString err;
    if (!m_service->stopAging(&err)) {
        QMessageBox::warning(this, QStringLiteral("Stop Aging Failed"), err);
    } else {
        updateStatusBanner();
    }
}

void SchedulingView::onIntervalChanged(int seconds)
{
    if (!m_service) return;
    QString err;
    if (!m_service->setAgingInterval(seconds, &err)) {
        QMessageBox::warning(this, QStringLiteral("Invalid Interval"), err);
        m_intervalSpin->setValue(m_service->agingInterval());
    } else {
        updateStatusBanner();
    }
}

void SchedulingView::onEvaluateAgingClicked()
{
    if (!m_service) return;
    int updated = 0;
    QString err;
    int evaluated = m_service->evaluateAging(QDateTime::currentDateTime(), &updated, &err);
    if (evaluated < 0) {
        QMessageBox::warning(this, QStringLiteral("Aging Evaluation Failed"), err);
    } else {
        refreshView();
    }
}

void SchedulingView::onDispatchClicked()
{
    if (!m_service) return;
    Models::Process dispatched;
    QString err;
    if (!m_service->dispatchNextProcess(&dispatched, &err)) {
        QMessageBox::information(this, QStringLiteral("Dispatch Info"), err);
    } else {
        refreshView();
    }
}

// ── Starvation Demonstration Implementation (Day 18) ─────────────────────────

void SchedulingView::onRunWithoutAgingClicked()
{
    if (!m_service) return;
    QString err;
    int interval = m_simAgingIntervalSpin->value();
    int horizon = m_simHorizonSpin->value();

    m_metricsWithoutAging = m_service->runStarvationSimulation(
        SchedulerComponents::SimulationMode::WithoutAging,
        interval,
        horizon,
        &err
    );

    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Simulation Error"), err);
        return;
    }

    m_hasRunWithoutAging = true;
    m_simStatusBadge->setText(QStringLiteral("🔴 Mode A (No Aging) Complete"));
    m_simStatusBadge->setStyleSheet(R"(
        QLabel {
            background-color: #ffe4e6;
            color: #be123c;
            border: 1px solid #fda4af;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 12px;
            font-weight: 600;
        }
    )");
    m_simClockLabel->setText(QStringLiteral("Simulated Time: %1s").arg(m_metricsWithoutAging.simulationTimeSec));

    renderSimulationTable(m_metricsWithoutAging);
    updateComparisonSummary();
}

void SchedulingView::onRunWithAgingClicked()
{
    if (!m_service) return;
    QString err;
    int interval = m_simAgingIntervalSpin->value();
    int horizon = m_simHorizonSpin->value();

    m_metricsWithAging = m_service->runStarvationSimulation(
        SchedulerComponents::SimulationMode::WithAging,
        interval,
        horizon,
        &err
    );

    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Simulation Error"), err);
        return;
    }

    m_hasRunWithAging = true;
    m_simStatusBadge->setText(QStringLiteral("🟢 Mode B (With Aging) Complete"));
    m_simStatusBadge->setStyleSheet(R"(
        QLabel {
            background-color: #dcfce7;
            color: #15803d;
            border: 1px solid #86efac;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 12px;
            font-weight: 600;
        }
    )");
    m_simClockLabel->setText(QStringLiteral("Simulated Time: %1s").arg(m_metricsWithAging.simulationTimeSec));

    renderSimulationTable(m_metricsWithAging);
    updateComparisonSummary();
}

void SchedulingView::onStepSimulationClicked()
{
    bool running = m_interactiveSim.step();
    SchedulerComponents::SimulationMetrics m = m_interactiveSim.currentMetrics();

    m_simClockLabel->setText(QStringLiteral("Simulated Time: %1s").arg(m_interactiveSim.currentSimulatedTimeSec()));
    m_simStatusBadge->setText(running ? QStringLiteral("🔵 Stepping: Active") : QStringLiteral("⚪ Stepping: Finished"));
    m_simStatusBadge->setStyleSheet(R"(
        QLabel {
            background-color: #e0f2fe;
            color: #0369a1;
            border: 1px solid #bae6fd;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 12px;
            font-weight: 600;
        }
    )");

    renderSimulationTable(m);
}

void SchedulingView::onResetSimulationClicked()
{
    SchedulerComponents::SimulationConfig config;
    config.mode = SchedulerComponents::SimulationMode::WithAging;
    config.agingIntervalSeconds = m_simAgingIntervalSpin ? m_simAgingIntervalSpin->value() : 30;
    config.simulationHorizonSeconds = m_simHorizonSpin ? m_simHorizonSpin->value() : 180;

    m_interactiveSim.setConfig(config);

    if (m_simClockLabel) {
        m_simClockLabel->setText(QStringLiteral("Simulated Time: 0s"));
    }
    if (m_simStatusBadge) {
        m_simStatusBadge->setText(QStringLiteral("⚪ Ready to Simulate"));
        m_simStatusBadge->setStyleSheet(R"(
            QLabel {
                background-color: #f1f5f9;
                color: #475569;
                border: 1px solid #cbd5e1;
                border-radius: 6px;
                padding: 6px 12px;
                font-size: 12px;
                font-weight: 600;
            }
        )");
    }

    renderSimulationTable(m_interactiveSim.currentMetrics());
}

void SchedulingView::updateComparisonSummary()
{
    // Col 1: Mode A (Without Aging)
    if (m_hasRunWithoutAging) {
        m_comparisonTable->setItem(0, 1, new QTableWidgetItem(QStringLiteral("AI-101 (PID 101)")));
        m_comparisonTable->setItem(1, 1, new QTableWidgetItem(QStringLiteral("%1 (Lowest)").arg(m_metricsWithoutAging.lowPriorityBasePriority)));
        m_comparisonTable->setItem(2, 1, new QTableWidgetItem(QStringLiteral("%1 (Unchanged)").arg(m_metricsWithoutAging.lowPriorityFinalEffectivePriority)));
        m_comparisonTable->setItem(3, 1, new QTableWidgetItem(QStringLiteral("%1s (Continuous wait)").arg(m_metricsWithoutAging.lowPriorityWaitingTime)));
        m_comparisonTable->setItem(4, 1, new QTableWidgetItem(QStringLiteral("%1 dispatches").arg(m_metricsWithoutAging.higherPriorityDispatchesBeforeLow)));

        QTableWidgetItem *dispItem = new QTableWidgetItem(m_metricsWithoutAging.lowPriorityDispatched ? QStringLiteral("Yes") : QStringLiteral("❌ No (Starved)"));
        dispItem->setForeground(QBrush(QColor(220, 38, 38)));
        dispItem->setFont([&]() { QFont f; f.setBold(true); return f; }());
        m_comparisonTable->setItem(5, 1, dispItem);

        QTableWidgetItem *outcomeItem = new QTableWidgetItem(m_metricsWithoutAging.starvationObserved ? QStringLiteral("🔴 Starvation Observed") : QStringLiteral("⚪ Horizon Reached"));
        outcomeItem->setForeground(QBrush(QColor(220, 38, 38)));
        outcomeItem->setFont([&]() { QFont f; f.setBold(true); return f; }());
        m_comparisonTable->setItem(6, 1, outcomeItem);
    } else {
        for (int r = 0; r < 7; ++r) {
            m_comparisonTable->setItem(r, 1, new QTableWidgetItem(QStringLiteral("Not run yet (Click 'Run WITHOUT Aging')")));
        }
    }

    // Col 2: Mode B (With Aging)
    if (m_hasRunWithAging) {
        m_comparisonTable->setItem(0, 2, new QTableWidgetItem(QStringLiteral("AI-101 (PID 101)")));
        m_comparisonTable->setItem(1, 2, new QTableWidgetItem(QStringLiteral("%1 (Lowest)").arg(m_metricsWithAging.lowPriorityBasePriority)));

        QTableWidgetItem *effItem = new QTableWidgetItem(QStringLiteral("%1 (Improved via aging)").arg(m_metricsWithAging.lowPriorityFinalEffectivePriority));
        effItem->setForeground(QBrush(QColor(21, 128, 61)));
        effItem->setFont([&]() { QFont f; f.setBold(true); return f; }());
        m_comparisonTable->setItem(2, 2, effItem);

        m_comparisonTable->setItem(3, 2, new QTableWidgetItem(QStringLiteral("%1s (Until dispatched)").arg(m_metricsWithAging.lowPriorityWaitingTime)));
        m_comparisonTable->setItem(4, 2, new QTableWidgetItem(QStringLiteral("%1 dispatches").arg(m_metricsWithAging.higherPriorityDispatchesBeforeLow)));

        QTableWidgetItem *dispItem = new QTableWidgetItem(m_metricsWithAging.lowPriorityDispatched ? QStringLiteral("✅ Yes (at t=%1s)").arg(m_metricsWithAging.lowPriorityWaitingTime) : QStringLiteral("No"));
        dispItem->setForeground(QBrush(QColor(21, 128, 61)));
        dispItem->setFont([&]() { QFont f; f.setBold(true); return f; }());
        m_comparisonTable->setItem(5, 2, dispItem);

        QTableWidgetItem *outcomeItem = new QTableWidgetItem(m_metricsWithAging.lowPriorityDispatched ? QStringLiteral("🟢 Starvation Mitigated") : QStringLiteral("⚪ Reached Horizon"));
        outcomeItem->setForeground(QBrush(QColor(21, 128, 61)));
        outcomeItem->setFont([&]() { QFont f; f.setBold(true); return f; }());
        m_comparisonTable->setItem(6, 2, outcomeItem);
    } else {
        for (int r = 0; r < 7; ++r) {
            m_comparisonTable->setItem(r, 2, new QTableWidgetItem(QStringLiteral("Not run yet (Click 'Run WITH Aging')")));
        }
    }
}

void SchedulingView::renderSimulationTable(const SchedulerComponents::SimulationMetrics &metrics)
{
    m_simProcessesTable->setRowCount(metrics.processSnapshots.size());

    QFont boldFont;
    boldFont.setBold(true);

    for (int row = 0; row < metrics.processSnapshots.size(); ++row) {
        const auto &snap = metrics.processSnapshots.at(row);

        // Col 0: PID
        QTableWidgetItem *pidItem = new QTableWidgetItem(QString::number(snap.flightId));
        pidItem->setTextAlignment(Qt::AlignCenter);
        m_simProcessesTable->setItem(row, 0, pidItem);

        // Col 1: Flight #
        QTableWidgetItem *fnItem = new QTableWidgetItem(snap.flightNumber);
        if (snap.flightId == 101) {
            fnItem->setFont(boldFont);
            fnItem->setForeground(QBrush(QColor(37, 99, 235)));
        }
        m_simProcessesTable->setItem(row, 1, fnItem);

        // Col 2: Airline
        m_simProcessesTable->setItem(row, 2, new QTableWidgetItem(snap.airline));

        // Col 3: Base Pri
        QTableWidgetItem *bpItem = new QTableWidgetItem(QString::number(snap.basePriority));
        bpItem->setTextAlignment(Qt::AlignCenter);
        m_simProcessesTable->setItem(row, 3, bpItem);

        // Col 4: Eff Pri
        QTableWidgetItem *epItem = new QTableWidgetItem(QString::number(snap.effectivePriority));
        epItem->setTextAlignment(Qt::AlignCenter);
        if (snap.effectivePriority < snap.basePriority) {
            epItem->setForeground(QBrush(QColor(21, 128, 61)));
            epItem->setFont(boldFont);
        }
        m_simProcessesTable->setItem(row, 4, epItem);

        // Col 5: Aging Level
        QTableWidgetItem *alItem = new QTableWidgetItem(QString::number(snap.agingLevel));
        alItem->setTextAlignment(Qt::AlignCenter);
        if (snap.agingLevel > 0) {
            alItem->setForeground(QBrush(QColor(21, 128, 61)));
            alItem->setFont(boldFont);
        }
        m_simProcessesTable->setItem(row, 5, alItem);

        // Col 6: Arrival (s)
        QTableWidgetItem *arrItem = new QTableWidgetItem(QStringLiteral("%1s").arg(snap.arrivalTimeSec));
        arrItem->setTextAlignment(Qt::AlignCenter);
        m_simProcessesTable->setItem(row, 6, arrItem);

        // Col 7: Wait Duration (s)
        QTableWidgetItem *waitItem = new QTableWidgetItem(QStringLiteral("%1s").arg(snap.waitingDurationSec));
        waitItem->setTextAlignment(Qt::AlignCenter);
        m_simProcessesTable->setItem(row, 7, waitItem);

        // Col 8: State
        QTableWidgetItem *stItem = new QTableWidgetItem(snap.stateString);
        stItem->setTextAlignment(Qt::AlignCenter);
        if (snap.stateString == QStringLiteral("Running")) {
            stItem->setForeground(QBrush(QColor(37, 99, 235)));
            stItem->setFont(boldFont);
        } else if (snap.stateString == QStringLiteral("Completed")) {
            stItem->setForeground(QBrush(QColor(21, 128, 61)));
        }
        m_simProcessesTable->setItem(row, 8, stItem);

        // Col 9: Dispatch Status
        QString dispText = snap.isDispatched ? QStringLiteral("Dispatched at t=%1s").arg(snap.dispatchTimeSec)
                                            : QStringLiteral("Pending in Queue");
        QTableWidgetItem *dispItem = new QTableWidgetItem(dispText);
        dispItem->setTextAlignment(Qt::AlignCenter);
        if (snap.isDispatched) {
            dispItem->setForeground(QBrush(QColor(21, 128, 61)));
        } else {
            dispItem->setForeground(QBrush(QColor(100, 116, 139)));
        }
        m_simProcessesTable->setItem(row, 9, dispItem);
    }
}
