#include "ui/DashboardView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QHeaderView>
#include <QDateTime>
#include <QGroupBox>
#include <QFrame>

DashboardView::DashboardView(ApplicationService *service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
    , m_refreshTimer(new QTimer(this))
{
    setupUi();

    if (m_service) {
        connect(m_service, &ApplicationService::flightsChanged, this, &DashboardView::refreshDashboard);
        connect(m_service, &ApplicationService::runwaysChanged, this, &DashboardView::refreshDashboard);
        connect(m_service, &ApplicationService::processesChanged, this, &DashboardView::refreshDashboard);
        connect(m_service, &ApplicationService::agingEvaluated, this, &DashboardView::refreshDashboard);
    }

    // Live clock and background periodic refresh (every 5 seconds)
    connect(m_refreshTimer, &QTimer::timeout, this, &DashboardView::onClockTick);
    m_refreshTimer->start(5000);

    onClockTick();
    refreshDashboard();
}

DashboardView::~DashboardView()
{
    if (m_refreshTimer) {
        m_refreshTimer->stop();
    }
}

void DashboardView::onClockTick()
{
    if (m_clockLabel) {
        m_clockLabel->setText(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd  hh:mm:ss")));
    }
}

void DashboardView::setupUi()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Header (Fixed at top)
    rootLayout->addWidget(createHeaderSection());

    // Scrollable content area
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background-color: #f8fafc; }"));

    QWidget *contentContainer = new QWidget(scrollArea);
    contentContainer->setStyleSheet(QStringLiteral("background-color: #f8fafc;"));
    QVBoxLayout *contentLayout = new QVBoxLayout(contentContainer);
    contentLayout->setContentsMargins(20, 16, 20, 24);
    contentLayout->setSpacing(18);

    // 1. KPI Cards (Rows 1 & 2)
    contentLayout->addWidget(createKpiSection());

    // 2. Emergency Flight Monitoring Section
    contentLayout->addWidget(createEmergencySection());

    // 3. Middle Section: Runway Utilization + Scheduling Overview
    contentLayout->addWidget(createMiddleSection());

    // 4. Bottom Section: Flight Activity + Transactions & Operation Logs
    contentLayout->addWidget(createBottomSection());

    scrollArea->setWidget(contentContainer);
    rootLayout->addWidget(scrollArea);
}

QWidget* DashboardView::createHeaderSection()
{
    QFrame *headerFrame = new QFrame(this);
    headerFrame->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #ffffff; border-bottom: 1px solid #e2e8f0; }"
    ));
    QHBoxLayout *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(24, 14, 24, 14);
    headerLayout->setSpacing(14);

    // Title & Subtitle
    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);

    QLabel *titleLabel = new QLabel(QStringLiteral("✈ SmartATC System Dashboard"), headerFrame);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: #0f172a;"));
    titleLayout->addWidget(titleLabel);

    QLabel *subLabel = new QLabel(QStringLiteral("Real-time Operational Overview • Air Traffic Control & Resource Allocation"), headerFrame);
    subLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: #64748b;"));
    titleLayout->addWidget(subLabel);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();

    // Clock
    m_clockLabel = new QLabel(headerFrame);
    m_clockLabel->setStyleSheet(QStringLiteral(
        "font-family: monospace; font-size: 13px; font-weight: 600; color: #334155; "
        "background-color: #f1f5f9; padding: 6px 12px; border-radius: 6px; border: 1px solid #cbd5e1;"
    ));
    headerLayout->addWidget(m_clockLabel);

    // DB Connection Badge
    m_dbBadge = new QLabel(headerFrame);
    m_dbBadge->setStyleSheet(QStringLiteral(
        "font-size: 12px; font-weight: 600; padding: 6px 12px; border-radius: 6px; "
        "background-color: #dcfce7; color: #166534; border: 1px solid #bbf7d0;"
    ));
    m_dbBadge->setText(QStringLiteral("● Database: Connected"));
    headerLayout->addWidget(m_dbBadge);

    // User / Role Badge
    m_userBadge = new QLabel(headerFrame);
    m_userBadge->setStyleSheet(QStringLiteral(
        "font-size: 12px; font-weight: 600; padding: 6px 12px; border-radius: 6px; "
        "background-color: #e0e7ff; color: #3730a3; border: 1px solid #c7d2fe;"
    ));
    headerLayout->addWidget(m_userBadge);

    // Manual Refresh Button
    m_refreshButton = new QPushButton(QStringLiteral("🔄 Refresh"), headerFrame);
    m_refreshButton->setCursor(Qt::PointingHandCursor);
    m_refreshButton->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2563eb; color: #ffffff; border: none; border-radius: 6px; "
        "padding: 7px 16px; font-weight: 600; font-size: 12px; }"
        "QPushButton:hover { background-color: #1d4ed8; }"
        "QPushButton:pressed { background-color: #1e40af; }"
    ));
    connect(m_refreshButton, &QPushButton::clicked, this, &DashboardView::refreshDashboard);
    headerLayout->addWidget(m_refreshButton);

    return headerFrame;
}

QWidget* DashboardView::createKpiCard(const QString &icon, const QString &title, QLabel *&valueLabel,
                                      const QString &accentColor, const QString &subtitle)
{
    QFrame *card = new QFrame(this);
    card->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #ffffff; border: 1px solid #e2e8f0; border-radius: 8px; }"
    ));
    card->setMinimumWidth(130);

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 12, 16, 12);
    cardLayout->setSpacing(4);

    // Top icon + title
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setSpacing(6);

    QLabel *iconLbl = new QLabel(icon, card);
    iconLbl->setStyleSheet(QStringLiteral("font-size: 14px;"));
    topLayout->addWidget(iconLbl);

    QLabel *titleLbl = new QLabel(title, card);
    titleLbl->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: 600; color: #64748b; text-transform: uppercase;"));
    titleLbl->setWordWrap(true);
    topLayout->addWidget(titleLbl);
    topLayout->addStretch();
    cardLayout->addLayout(topLayout);

    // Value
    valueLabel = new QLabel(QStringLiteral("0"), card);
    valueLabel->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 700; color: %1;").arg(accentColor));
    cardLayout->addWidget(valueLabel);

    // Subtitle
    if (!subtitle.isEmpty()) {
        QLabel *subLbl = new QLabel(subtitle, card);
        subLbl->setStyleSheet(QStringLiteral("font-size: 11px; color: #94a3b8;"));
        subLbl->setWordWrap(true);
        cardLayout->addWidget(subLbl);
    }

    return card;
}

QWidget* DashboardView::createKpiSection()
{
    QWidget *kpiContainer = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(kpiContainer);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(10);

    // Row 1: Flights & Core Runway count
    QHBoxLayout *row1 = new QHBoxLayout();
    row1->setSpacing(12);

    row1->addWidget(createKpiCard(QStringLiteral("✈"), QStringLiteral("Total Flights"), m_lblTotalFlights, QStringLiteral("#0f172a"), QStringLiteral("All registered flight records")));
    row1->addWidget(createKpiCard(QStringLiteral("📅"), QStringLiteral("Scheduled"), m_lblScheduledFlights, QStringLiteral("#2563eb"), QStringLiteral("Awaiting active clearance")));
    row1->addWidget(createKpiCard(QStringLiteral("🛫"), QStringLiteral("Active Flights"), m_lblActiveFlights, QStringLiteral("#059669"), QStringLiteral("Scheduled, Taxiing, Cleared")));
    row1->addWidget(createKpiCard(QStringLiteral("🚨"), QStringLiteral("Emergency"), m_lblEmergencyFlights, QStringLiteral("#dc2626"), QStringLiteral("Priority 1 preemptive state")));
    row1->addWidget(createKpiCard(QStringLiteral("🛣"), QStringLiteral("Total Runways"), m_lblTotalRunways, QStringLiteral("#475569"), QStringLiteral("Physical runway assets")));

    vbox->addLayout(row1);

    // Row 2: Physical Runway Asset States
    QHBoxLayout *row2 = new QHBoxLayout();
    row2->setSpacing(12);

    row2->addWidget(createKpiCard(QStringLiteral("🟢"), QStringLiteral("Available Runways"), m_lblAvailableRunways, QStringLiteral("#16a34a"), QStringLiteral("Ready for allocation")));
    row2->addWidget(createKpiCard(QStringLiteral("🔴"), QStringLiteral("Occupied Runways"), m_lblOccupiedRunways, QStringLiteral("#d97706"), QStringLiteral("Currently active")));
    row2->addWidget(createKpiCard(QStringLiteral("🟡"), QStringLiteral("Maintenance"), m_lblMaintenanceRunways, QStringLiteral("#64748b"), QStringLiteral("Offline for repairs")));
    row2->addWidget(createKpiCard(QStringLiteral("📊"), QStringLiteral("Utilization"), m_lblRunwayUtilization, QStringLiteral("#4f46e5"), QStringLiteral("(Occupied / Total) × 100")));

    vbox->addLayout(row2);

    // Row 3: OS Process Queue & Dispatch Simulation Metrics
    QHBoxLayout *row3 = new QHBoxLayout();
    row3->setSpacing(12);

    row3->addWidget(createKpiCard(QStringLiteral("⏳"), QStringLiteral("Ready Queue"), m_lblReadyQueue, QStringLiteral("#7c3aed"), QStringLiteral("Waiting OS processes")));
    row3->addWidget(createKpiCard(QStringLiteral("⚙"), QStringLiteral("Running CPU"), m_lblRunningProcesses, QStringLiteral("#0284c7"), QStringLiteral("Active dispatch")));
    row3->addWidget(createKpiCard(QStringLiteral("✅"), QStringLiteral("Completed"), m_lblCompletedProcesses, QStringLiteral("#059669"), QStringLiteral("Turnaround recorded")));

    vbox->addLayout(row3);
    return kpiContainer;
}

QWidget* DashboardView::createEmergencySection()
{
    m_emergencyCard = new QFrame(this);
    m_emergencyCard->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #ffffff; border: 1px solid #fecaca; border-radius: 8px; }"
    ));

    QVBoxLayout *layout = new QVBoxLayout(m_emergencyCard);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(10);

    // Banner
    m_emergencyBanner = new QLabel(m_emergencyCard);
    m_emergencyBanner->setStyleSheet(QStringLiteral(
        "font-size: 13px; font-weight: 600; padding: 8px 12px; border-radius: 6px; "
        "background-color: #fef2f2; color: #991b1b; border: 1px solid #fee2e2;"
    ));
    layout->addWidget(m_emergencyBanner);

    // Emergency Table
    m_emergencyTable = new QTableWidget(m_emergencyCard);
    m_emergencyTable->setColumnCount(6);
    m_emergencyTable->setHorizontalHeaderLabels({
        QStringLiteral("Flight #"), QStringLiteral("Airline"), QStringLiteral("Route"),
        QStringLiteral("Base Prio"), QStringLiteral("Effective Prio"), QStringLiteral("Status")
    });
    m_emergencyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_emergencyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_emergencyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_emergencyTable->verticalHeader()->setVisible(false);
    m_emergencyTable->setMaximumHeight(150);
    m_emergencyTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background-color: #ffffff; border: 1px solid #fee2e2; border-radius: 6px; } "
        "QHeaderView::section { background-color: #fef2f2; color: #991b1b; font-weight: 600; padding: 6px; border: none; }"
    ));
    layout->addWidget(m_emergencyTable);

    return m_emergencyCard;
}

QWidget* DashboardView::createMiddleSection()
{
    QWidget *container = new QWidget(this);
    QHBoxLayout *hlayout = new QHBoxLayout(container);
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setSpacing(16);

    // ── Left: Runway Resource Allocation & Utilization ─────────────────────────
    QFrame *rwFrame = new QFrame(container);
    rwFrame->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #ffffff; border: 1px solid #e2e8f0; border-radius: 8px; }"
    ));
    QVBoxLayout *rwLayout = new QVBoxLayout(rwFrame);
    rwLayout->setContentsMargins(18, 16, 18, 16);
    rwLayout->setSpacing(10);

    QHBoxLayout *rwHeader = new QHBoxLayout();
    QLabel *rwTitle = new QLabel(QStringLiteral("🛣 Runway Resource Utilization"), rwFrame);
    rwTitle->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700; color: #0f172a;"));
    rwHeader->addWidget(rwTitle);
    rwHeader->addStretch();

    m_btnNavRunways = new QPushButton(QStringLiteral("Manage Runways →"), rwFrame);
    m_btnNavRunways->setCursor(Qt::PointingHandCursor);
    m_btnNavRunways->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #f1f5f9; color: #2563eb; border: 1px solid #cbd5e1; border-radius: 4px; "
        "padding: 4px 10px; font-weight: 600; font-size: 11px; }"
        "QPushButton:hover { background-color: #e2e8f0; }"
    ));
    connect(m_btnNavRunways, &QPushButton::clicked, this, &DashboardView::navigateToRunways);
    rwHeader->addWidget(m_btnNavRunways);
    rwLayout->addLayout(rwHeader);

    // Progress bar
    m_runwayUtilProgress = new QProgressBar(rwFrame);
    m_runwayUtilProgress->setRange(0, 100);
    m_runwayUtilProgress->setValue(0);
    m_runwayUtilProgress->setTextVisible(true);
    m_runwayUtilProgress->setFixedHeight(20);
    m_runwayUtilProgress->setStyleSheet(QStringLiteral(
        "QProgressBar { background-color: #f1f5f9; border: 1px solid #cbd5e1; border-radius: 4px; text-align: center; font-weight: 600; } "
        "QProgressBar::chunk { background-color: #2563eb; border-radius: 3px; }"
    ));
    rwLayout->addWidget(m_runwayUtilProgress);

    m_runwayUtilLabel = new QLabel(QStringLiteral("Overall Utilization: 0.0%"), rwFrame);
    m_runwayUtilLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: #64748b; font-weight: 500;"));
    rwLayout->addWidget(m_runwayUtilLabel);

    // Runway overview table
    m_runwayTable = new QTableWidget(rwFrame);
    m_runwayTable->setColumnCount(4);
    m_runwayTable->setHorizontalHeaderLabels({
        QStringLiteral("Runway"), QStringLiteral("Type"), QStringLiteral("Status"), QStringLiteral("Assigned Flight")
    });
    m_runwayTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_runwayTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_runwayTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_runwayTable->verticalHeader()->setVisible(false);
    m_runwayTable->setMinimumHeight(180);
    m_runwayTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background-color: #ffffff; border: 1px solid #e2e8f0; border-radius: 6px; } "
        "QHeaderView::section { background-color: #f8fafc; color: #475569; font-weight: 600; padding: 6px; border: none; }"
    ));
    rwLayout->addWidget(m_runwayTable);

    hlayout->addWidget(rwFrame, 1);

    // ── Right: OS Process Scheduling & ReadyQueue Monitoring ─────────────────
    QFrame *schedFrame = new QFrame(container);
    schedFrame->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #ffffff; border: 1px solid #e2e8f0; border-radius: 8px; }"
    ));
    QVBoxLayout *schedLayout = new QVBoxLayout(schedFrame);
    schedLayout->setContentsMargins(18, 16, 18, 16);
    schedLayout->setSpacing(10);

    QHBoxLayout *schedHeader = new QHBoxLayout();
    QLabel *schedTitle = new QLabel(QStringLiteral("⚙ OS Process Scheduling & ReadyQueue"), schedFrame);
    schedTitle->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700; color: #0f172a;"));
    schedHeader->addWidget(schedTitle);
    schedHeader->addStretch();

    m_btnNavScheduling = new QPushButton(QStringLiteral("Open Scheduling →"), schedFrame);
    m_btnNavScheduling->setCursor(Qt::PointingHandCursor);
    m_btnNavScheduling->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #f1f5f9; color: #4f46e5; border: 1px solid #cbd5e1; border-radius: 4px; "
        "padding: 4px 10px; font-weight: 600; font-size: 11px; }"
        "QPushButton:hover { background-color: #e2e8f0; }"
    ));
    connect(m_btnNavScheduling, &QPushButton::clicked, this, &DashboardView::navigateToScheduling);
    schedHeader->addWidget(m_btnNavScheduling);
    schedLayout->addLayout(schedHeader);

    // Policy Badge
    m_lblSchedPolicy = new QLabel(schedFrame);
    m_lblSchedPolicy->setStyleSheet(QStringLiteral(
        "background-color: #eef2ff; color: #3730a3; padding: 6px 10px; border-radius: 6px; "
        "font-size: 11px; font-weight: 600; border: 1px solid #c7d2fe;"
    ));
    schedLayout->addWidget(m_lblSchedPolicy);

    // Aging status & metrics row
    QHBoxLayout *metricsRow = new QHBoxLayout();
    m_lblAgingStatus = new QLabel(schedFrame);
    m_lblAgingStatus->setStyleSheet(QStringLiteral("font-size: 11px; color: #475569; font-weight: 600;"));
    metricsRow->addWidget(m_lblAgingStatus);
    metricsRow->addStretch();

    m_lblAvgWaitTime = new QLabel(schedFrame);
    m_lblAvgWaitTime->setStyleSheet(QStringLiteral("font-size: 11px; color: #64748b; font-weight: 500;"));
    metricsRow->addWidget(m_lblAvgWaitTime);

    m_lblAvgTurnaroundTime = new QLabel(schedFrame);
    m_lblAvgTurnaroundTime->setStyleSheet(QStringLiteral("font-size: 11px; color: #64748b; font-weight: 500;"));
    metricsRow->addWidget(m_lblAvgTurnaroundTime);
    schedLayout->addLayout(metricsRow);

    // Next candidate card
    QFrame *candCard = new QFrame(schedFrame);
    candCard->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; padding: 8px; }"
    ));
    QVBoxLayout *candLayout = new QVBoxLayout(candCard);
    candLayout->setContentsMargins(10, 8, 10, 8);
    candLayout->setSpacing(4);

    QLabel *candHeaderLbl = new QLabel(QStringLiteral("NEXT ELIGIBLE DISPATCH CANDIDATE"), candCard);
    candHeaderLbl->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: 700; color: #64748b; text-transform: uppercase;"));
    candLayout->addWidget(candHeaderLbl);

    m_lblNextCandidate = new QLabel(QStringLiteral("None (ReadyQueue is empty)"), candCard);
    m_lblNextCandidate->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 600; color: #0f172a;"));
    candLayout->addWidget(m_lblNextCandidate);
    schedLayout->addWidget(candCard);

    schedLayout->addStretch();
    hlayout->addWidget(schedFrame, 1);

    return container;
}

QWidget* DashboardView::createBottomSection()
{
    QWidget *container = new QWidget(this);
    QHBoxLayout *hlayout = new QHBoxLayout(container);
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setSpacing(16);

    // ── Left: Recent Flight Activity ──────────────────────────────────────────
    QFrame *flightFrame = new QFrame(container);
    flightFrame->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #ffffff; border: 1px solid #e2e8f0; border-radius: 8px; }"
    ));
    QVBoxLayout *flLayout = new QVBoxLayout(flightFrame);
    flLayout->setContentsMargins(18, 16, 18, 16);
    flLayout->setSpacing(10);

    QHBoxLayout *flHeader = new QHBoxLayout();
    QLabel *flTitle = new QLabel(QStringLiteral("✈ Recent Flight Activity"), flightFrame);
    flTitle->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700; color: #0f172a;"));
    flHeader->addWidget(flTitle);
    flHeader->addStretch();

    m_btnNavFlights = new QPushButton(QStringLiteral("Manage Flights →"), flightFrame);
    m_btnNavFlights->setCursor(Qt::PointingHandCursor);
    m_btnNavFlights->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #f1f5f9; color: #2563eb; border: 1px solid #cbd5e1; border-radius: 4px; "
        "padding: 4px 10px; font-weight: 600; font-size: 11px; }"
        "QPushButton:hover { background-color: #e2e8f0; }"
    ));
    connect(m_btnNavFlights, &QPushButton::clicked, this, &DashboardView::navigateToFlights);
    flHeader->addWidget(m_btnNavFlights);
    flLayout->addLayout(flHeader);

    m_flightsTable = new QTableWidget(flightFrame);
    m_flightsTable->setColumnCount(6);
    m_flightsTable->setHorizontalHeaderLabels({
        QStringLiteral("Flight #"), QStringLiteral("Airline"), QStringLiteral("Type"),
        QStringLiteral("Status"), QStringLiteral("Prio"), QStringLiteral("Runway")
    });
    m_flightsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_flightsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_flightsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_flightsTable->verticalHeader()->setVisible(false);
    m_flightsTable->setMinimumHeight(220);
    m_flightsTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background-color: #ffffff; border: 1px solid #e2e8f0; border-radius: 6px; } "
        "QHeaderView::section { background-color: #f8fafc; color: #475569; font-weight: 600; padding: 6px; border: none; }"
    ));
    flLayout->addWidget(m_flightsTable);
    hlayout->addWidget(flightFrame, 1);

    // ── Right: Database Transactions & Audit Logs ────────────────────────────
    QFrame *txFrame = new QFrame(container);
    txFrame->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #ffffff; border: 1px solid #e2e8f0; border-radius: 8px; }"
    ));
    QVBoxLayout *txLayout = new QVBoxLayout(txFrame);
    txLayout->setContentsMargins(18, 16, 18, 16);
    txLayout->setSpacing(10);

    QLabel *txTitle = new QLabel(QStringLiteral("🔒 Database Transactions & Audit Logs"), txFrame);
    txTitle->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700; color: #0f172a;"));
    txLayout->addWidget(txTitle);

    // Last Transaction Card
    QFrame *lastTxCard = new QFrame(txFrame);
    lastTxCard->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; padding: 6px; }"
    ));
    QVBoxLayout *ltLayout = new QVBoxLayout(lastTxCard);
    ltLayout->setContentsMargins(10, 8, 10, 8);
    ltLayout->setSpacing(4);

    QHBoxLayout *ltHeader = new QHBoxLayout();
    m_lastTxOperation = new QLabel(QStringLiteral("LAST TRANSACTION: NONE"), lastTxCard);
    m_lastTxOperation->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: 700; color: #475569;"));
    ltHeader->addWidget(m_lastTxOperation);
    ltHeader->addStretch();

    m_lastTxStatus = new QLabel(QStringLiteral("N/A"), lastTxCard);
    m_lastTxStatus->setStyleSheet(QStringLiteral(
        "font-size: 10px; font-weight: 700; padding: 2px 8px; border-radius: 4px; background-color: #e2e8f0; color: #475569;"
    ));
    ltHeader->addWidget(m_lastTxStatus);
    ltLayout->addLayout(ltHeader);

    m_lastTxDetails = new QLabel(QStringLiteral("No runway transactions recorded in this session."), lastTxCard);
    m_lastTxDetails->setStyleSheet(QStringLiteral("font-size: 12px; color: #0f172a;"));
    m_lastTxDetails->setWordWrap(true);
    ltLayout->addWidget(m_lastTxDetails);

    m_lastTxTime = new QLabel(QStringLiteral("—"), lastTxCard);
    m_lastTxTime->setStyleSheet(QStringLiteral("font-size: 10px; color: #94a3b8;"));
    ltLayout->addWidget(m_lastTxTime);

    txLayout->addWidget(lastTxCard);

    // Recent Operation Logs Table
    m_logsTable = new QTableWidget(txFrame);
    m_logsTable->setColumnCount(4);
    m_logsTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("Action"), QStringLiteral("Status"), QStringLiteral("Time")
    });
    m_logsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_logsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logsTable->verticalHeader()->setVisible(false);
    m_logsTable->setMinimumHeight(150);
    m_logsTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background-color: #ffffff; border: 1px solid #e2e8f0; border-radius: 6px; } "
        "QHeaderView::section { background-color: #f8fafc; color: #475569; font-weight: 600; padding: 6px; border: none; }"
    ));
    txLayout->addWidget(m_logsTable);
    hlayout->addWidget(txFrame, 1);

    return container;
}

void DashboardView::refreshDashboard()
{
    if (!m_service) {
        return;
    }

    // 1. Session & DB Status
    if (m_userBadge) {
        QString user = m_service->currentSession().username();
        QString role = m_service->currentSession().role();
        m_userBadge->setText(QStringLiteral("👤 %1 [%2]").arg(user.isEmpty() ? QStringLiteral("Guest") : user).arg(role.isEmpty() ? QStringLiteral("Unauthenticated") : role));
    }

    bool isConnected = m_service->checkDatabaseConnection();
    if (m_dbBadge) {
        if (isConnected) {
            m_dbBadge->setText(QStringLiteral("● Database: Connected (smartatc)"));
            m_dbBadge->setStyleSheet(QStringLiteral(
                "font-size: 12px; font-weight: 600; padding: 6px 12px; border-radius: 6px; "
                "background-color: #dcfce7; color: #166534; border: 1px solid #bbf7d0;"
            ));
        } else {
            m_dbBadge->setText(QStringLiteral("○ Database: Offline"));
            m_dbBadge->setStyleSheet(QStringLiteral(
                "font-size: 12px; font-weight: 600; padding: 6px 12px; border-radius: 6px; "
                "background-color: #fee2e2; color: #991b1b; border: 1px solid #fecaca;"
            ));
        }
    }

    // 2. Aggregate Data Summary
    QString err;
    ApplicationService::DashboardSummary summary = m_service->getDashboardSummary(&err);

    // 3. Update KPI Labels
    m_lblTotalFlights->setText(QString::number(summary.totalFlights));
    m_lblScheduledFlights->setText(QString::number(summary.scheduledFlights));
    m_lblActiveFlights->setText(QString::number(summary.activeFlights));
    m_lblEmergencyFlights->setText(QString::number(summary.emergencyFlights));
    m_lblTotalRunways->setText(QString::number(summary.totalRunways));

    m_lblAvailableRunways->setText(QString::number(summary.availableRunways));
    m_lblOccupiedRunways->setText(QString::number(summary.occupiedRunways));
    m_lblMaintenanceRunways->setText(QString::number(summary.maintenanceRunways));
    m_lblRunwayUtilization->setText(QStringLiteral("%1%").arg(summary.runwayUtilization, 0, 'f', 1));

    m_lblReadyQueue->setText(QString::number(summary.readyQueueDepth));
    m_lblRunningProcesses->setText(QString::number(summary.runningProcesses));
    m_lblCompletedProcesses->setText(QString::number(summary.completedProcesses));

    // 4. Update Emergency Section
    if (summary.emergencyFlights > 0) {
        m_emergencyCard->setVisible(true);
        m_emergencyBanner->setText(QStringLiteral("🚨 ATTENTION: %1 In-Flight Emergency Declaration(s) Active! Elevated to Priority 1.").arg(summary.emergencyFlights));
        m_emergencyTable->setVisible(true);
        m_emergencyTable->setRowCount(summary.activeEmergencyFlights.size());

        for (int r = 0; r < summary.activeEmergencyFlights.size(); ++r) {
            const Models::Flight &ef = summary.activeEmergencyFlights.at(r);
            m_emergencyTable->setItem(r, 0, new QTableWidgetItem(ef.flightNumber));
            m_emergencyTable->setItem(r, 1, new QTableWidgetItem(ef.airline));
            m_emergencyTable->setItem(r, 2, new QTableWidgetItem(QStringLiteral("%1 → %2").arg(ef.effectiveSource()).arg(ef.destination)));
            m_emergencyTable->setItem(r, 3, new QTableWidgetItem(QString::number(ef.priority)));

            QTableWidgetItem *effItem = new QTableWidgetItem(QStringLiteral("1 - 🚨 EMERGENCY"));
            effItem->setForeground(QBrush(QColor(QStringLiteral("#dc2626"))));
            m_emergencyTable->setItem(r, 4, effItem);

            m_emergencyTable->setItem(r, 5, new QTableWidgetItem(ef.operationalStatus));
        }
    } else {
        m_emergencyCard->setVisible(true);
        m_emergencyBanner->setText(QStringLiteral("✅ Normal Operations — No in-flight emergencies declared."));
        m_emergencyBanner->setStyleSheet(QStringLiteral(
            "font-size: 13px; font-weight: 600; padding: 8px 12px; border-radius: 6px; "
            "background-color: #f0fdf4; color: #166534; border: 1px solid #bbf7d0;"
        ));
        m_emergencyTable->setVisible(false);
    }

    // 5. Update Runway Section
    m_runwayUtilProgress->setValue(static_cast<int>(summary.runwayUtilization));
    m_runwayUtilLabel->setText(QStringLiteral("Runway Utilization: %1% (%2 Occupied / %3 Total)")
                                   .arg(summary.runwayUtilization, 0, 'f', 1)
                                   .arg(summary.occupiedRunways)
                                   .arg(summary.totalRunways));

    m_runwayTable->setRowCount(summary.runwaysOverview.size());
    for (int r = 0; r < summary.runwaysOverview.size(); ++r) {
        const Models::Runway &rw = summary.runwaysOverview.at(r);
        m_runwayTable->setItem(r, 0, new QTableWidgetItem(rw.runwayNumber));
        m_runwayTable->setItem(r, 1, new QTableWidgetItem(rw.runwayType));

        QTableWidgetItem *stItem = new QTableWidgetItem(rw.status);
        if (rw.status == QStringLiteral("Available")) {
            stItem->setForeground(QBrush(QColor(QStringLiteral("#16a34a"))));
        } else if (rw.status == QStringLiteral("Occupied")) {
            stItem->setForeground(QBrush(QColor(QStringLiteral("#d97706"))));
        } else {
            stItem->setForeground(QBrush(QColor(QStringLiteral("#dc2626"))));
        }
        m_runwayTable->setItem(r, 2, stItem);

        // Assigned flight
        Models::RunwayAssignment ra;
        QString flightText = QStringLiteral("—");
        if (m_service->getActiveAssignmentForRunway(rw.id, ra)) {
            flightText = ra.flightNumber.isEmpty() ? QStringLiteral("Flight #%1").arg(ra.flightId) : ra.flightNumber;
        }
        m_runwayTable->setItem(r, 3, new QTableWidgetItem(flightText));
    }

    // 6. Update Scheduling Section
    m_lblSchedPolicy->setText(summary.schedulingPolicy);
    m_lblAgingStatus->setText(QStringLiteral("Priority Aging: %1 (Interval: %2s)")
                                  .arg(summary.isAgingActive ? QStringLiteral("Active ▶") : QStringLiteral("Inactive ⏹"))
                                  .arg(summary.agingIntervalSeconds));
    m_lblAvgWaitTime->setText(QStringLiteral("Avg WT: %1s").arg(summary.averageWaitingTime, 0, 'f', 1));
    m_lblAvgTurnaroundTime->setText(QStringLiteral("Avg TAT: %1s").arg(summary.averageTurnaroundTime, 0, 'f', 1));

    if (summary.hasHighestPriorityWaiting) {
        m_lblNextCandidate->setText(QStringLiteral("PID %1 | Flight #%2 | Effective Prio: %3 (Base: %4)")
                                        .arg(summary.highestPriorityWaitingProcess.flightId)
                                        .arg(summary.highestPriorityWaitingProcess.flightNumber.isEmpty() ? QString::number(summary.highestPriorityWaitingProcess.flightId) : summary.highestPriorityWaitingProcess.flightNumber)
                                        .arg(summary.highestPriorityWaitingProcess.effectivePriority)
                                        .arg(summary.highestPriorityWaitingProcess.basePriority));
    } else {
        m_lblNextCandidate->setText(QStringLiteral("None (ReadyQueue is empty)"));
    }

    // 7. Update Recent Flights Table
    m_flightsTable->setRowCount(summary.recentFlights.size());
    for (int r = 0; r < summary.recentFlights.size(); ++r) {
        const Models::Flight &f = summary.recentFlights.at(r);
        m_flightsTable->setItem(r, 0, new QTableWidgetItem(f.flightNumber));
        m_flightsTable->setItem(r, 1, new QTableWidgetItem(f.airline));
        m_flightsTable->setItem(r, 2, new QTableWidgetItem(f.flightType));
        m_flightsTable->setItem(r, 3, new QTableWidgetItem(f.operationalStatus));

        QTableWidgetItem *prioItem = new QTableWidgetItem(QString::number(f.effectivePriority()));
        if (f.isEmergency) {
            prioItem->setForeground(QBrush(QColor(QStringLiteral("#dc2626"))));
            prioItem->setText(QStringLiteral("1 (🚨)"));
        }
        m_flightsTable->setItem(r, 4, prioItem);

        Models::RunwayAssignment ra;
        QString rwText = QStringLiteral("—");
        if (m_service->getActiveAssignmentForFlight(f.id, ra)) {
            rwText = ra.runwayNumber.isEmpty() ? QStringLiteral("RW #%1").arg(ra.runwayId) : ra.runwayNumber;
        }
        m_flightsTable->setItem(r, 5, new QTableWidgetItem(rwText));
    }

    // 8. Update Transactions & Audit Logs
    if (!summary.lastTx.operation.isEmpty()) {
        m_lastTxOperation->setText(QStringLiteral("LAST TRANSACTION: %1").arg(summary.lastTx.operation));
        m_lastTxStatus->setText(summary.lastTx.status);
        if (summary.lastTx.status == QStringLiteral("Committed")) {
            m_lastTxStatus->setStyleSheet(QStringLiteral(
                "font-size: 10px; font-weight: 700; padding: 2px 8px; border-radius: 4px; "
                "background-color: #dcfce7; color: #166534;"
            ));
        } else {
            m_lastTxStatus->setStyleSheet(QStringLiteral(
                "font-size: 10px; font-weight: 700; padding: 2px 8px; border-radius: 4px; "
                "background-color: #fee2e2; color: #991b1b;"
            ));
        }
        m_lastTxDetails->setText(summary.lastTx.message.isEmpty()
                                     ? QStringLiteral("Runway ID: %1, Flight ID: %2").arg(summary.lastTx.runwayId).arg(summary.lastTx.flightId)
                                     : summary.lastTx.message);
        m_lastTxTime->setText(summary.lastTx.timestamp);
    } else {
        m_lastTxOperation->setText(QStringLiteral("LAST TRANSACTION: NONE"));
        m_lastTxStatus->setText(QStringLiteral("N/A"));
        m_lastTxDetails->setText(QStringLiteral("No runway transactions recorded in this session."));
        m_lastTxTime->setText(QStringLiteral("—"));
    }

    m_logsTable->setRowCount(summary.recentLogs.size());
    for (int r = 0; r < summary.recentLogs.size(); ++r) {
        const ApplicationService::OperationLogEntry &log = summary.recentLogs.at(r);
        m_logsTable->setItem(r, 0, new QTableWidgetItem(QString::number(log.logId)));
        m_logsTable->setItem(r, 1, new QTableWidgetItem(log.operationType));

        QTableWidgetItem *stItem = new QTableWidgetItem(log.transactionStatus);
        if (log.transactionStatus == QStringLiteral("Committed")) {
            stItem->setForeground(QBrush(QColor(QStringLiteral("#166534"))));
        } else if (log.transactionStatus == QStringLiteral("Rolled Back")) {
            stItem->setForeground(QBrush(QColor(QStringLiteral("#991b1b"))));
        }
        m_logsTable->setItem(r, 2, stItem);
        m_logsTable->setItem(r, 3, new QTableWidgetItem(log.createdAt.toString(QStringLiteral("hh:mm:ss"))));
    }
}

