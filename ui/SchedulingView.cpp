#include "ui/SchedulingView.h"
#include "services/ApplicationService.h"
#include "scheduler/Scheduler.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QFont>

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
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // ── 1. Header & Title ────────────────────────────────────────────────────
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QLabel *titleLabel = new QLabel(QStringLiteral("⏱ Priority Scheduling & Aging Monitor"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet(QStringLiteral("color: #0f172a;"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_statusBadge = new QLabel(QStringLiteral("⚪ Aging: Stopped"), this);
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
    mainLayout->addLayout(headerLayout);

    // ── 2. Toolbar & Controls ────────────────────────────────────────────────
    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    toolbarLayout->setSpacing(10);

    QLabel *intervalLabel = new QLabel(QStringLiteral("Interval:"), this);
    intervalLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: #334155; font-size: 13px;"));
    toolbarLayout->addWidget(intervalLabel);

    m_intervalSpin = new QSpinBox(this);
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

    m_startAgingButton = new QPushButton(QStringLiteral("▶ Start Aging"), this);
    m_startAgingButton->setStyleSheet(R"(
        QPushButton {
            background-color: #059669;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 7px 14px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover { background-color: #047857; }
        QPushButton:disabled { background-color: #cbd5e1; color: #94a3b8; }
    )");
    connect(m_startAgingButton, &QPushButton::clicked, this, &SchedulingView::onStartAgingClicked);
    toolbarLayout->addWidget(m_startAgingButton);

    m_stopAgingButton = new QPushButton(QStringLiteral("⏹ Stop Aging"), this);
    m_stopAgingButton->setEnabled(false);
    m_stopAgingButton->setStyleSheet(R"(
        QPushButton {
            background-color: #dc2626;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 7px 14px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover { background-color: #b91c1c; }
        QPushButton:disabled { background-color: #cbd5e1; color: #94a3b8; }
    )");
    connect(m_stopAgingButton, &QPushButton::clicked, this, &SchedulingView::onStopAgingClicked);
    toolbarLayout->addWidget(m_stopAgingButton);

    m_evalAgingButton = new QPushButton(QStringLiteral("⚡ Evaluate Aging"), this);
    m_evalAgingButton->setStyleSheet(R"(
        QPushButton {
            background-color: #4f46e5;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 7px 14px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover { background-color: #4338ca; }
        QPushButton:disabled { background-color: #cbd5e1; color: #94a3b8; }
    )");
    connect(m_evalAgingButton, &QPushButton::clicked, this, &SchedulingView::onEvaluateAgingClicked);
    toolbarLayout->addWidget(m_evalAgingButton);

    toolbarLayout->addStretch();

    m_dispatchButton = new QPushButton(QStringLiteral("🚀 Dispatch Process"), this);
    m_dispatchButton->setStyleSheet(R"(
        QPushButton {
            background-color: #0284c7;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 7px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover { background-color: #0369a1; }
        QPushButton:disabled { background-color: #cbd5e1; color: #94a3b8; }
    )");
    connect(m_dispatchButton, &QPushButton::clicked, this, &SchedulingView::onDispatchClicked);
    toolbarLayout->addWidget(m_dispatchButton);

    m_refreshButton = new QPushButton(QStringLiteral("🔄 Refresh"), this);
    m_refreshButton->setStyleSheet(R"(
        QPushButton {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 7px 14px;
            font-size: 13px;
            font-weight: 500;
            color: #334155;
        }
        QPushButton:hover { background-color: #f1f5f9; }
    )");
    connect(m_refreshButton, &QPushButton::clicked, this, &SchedulingView::refreshView);
    toolbarLayout->addWidget(m_refreshButton);

    mainLayout->addLayout(toolbarLayout);

    // Permission notice
    m_permNotice = new QLabel(this);
    m_permNotice->setVisible(false);
    m_permNotice->setStyleSheet(QStringLiteral("color: #b45309; font-size: 12px; font-weight: 500; background-color: #fef3c7; border: 1px solid #fde68a; border-radius: 4px; padding: 4px 8px;"));
    mainLayout->addWidget(m_permNotice);

    // ── 3. Running Process Banner ─────────────────────────────────────────────
    m_runningBanner = new QLabel(QStringLiteral("CPU / Runway Dispatcher: IDLE (No process running)"), this);
    m_runningBanner->setStyleSheet(R"(
        QLabel {
            background-color: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            padding: 10px 14px;
            font-size: 13px;
            font-weight: 600;
            color: #1e293b;
        }
    )");
    mainLayout->addWidget(m_runningBanner);

    // ── 4. Table ─────────────────────────────────────────────────────────────
    m_tableWidget = new QTableWidget(this);
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
            padding: 10px 12px;
            border: none;
            border-bottom: 2px solid #e2e8f0;
        }
    )");

    mainLayout->addWidget(m_tableWidget);
}

void SchedulingView::updatePermissions()
{
    if (!m_service) {
        return;
    }

    bool canManage = m_service->currentSession().hasPermission(QStringLiteral("action:ManageScheduling"));
    m_startAgingButton->setEnabled(canManage && !m_service->isAgingRunning());
    m_stopAgingButton->setEnabled(canManage && m_service->isAgingRunning());
    m_intervalSpin->setEnabled(canManage);
    m_evalAgingButton->setEnabled(canManage);
    m_dispatchButton->setEnabled(canManage);

    if (!canManage) {
        m_permNotice->setText(QStringLiteral("Notice: Role '%1' possesses view-only access. Priority aging and scheduling controls are restricted to ATC and Administrator roles.")
                                  .arg(m_service->currentSession().role()));
        m_permNotice->setVisible(true);
    } else {
        m_permNotice->setVisible(false);
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
        m_statusBadge->setText(QStringLiteral("🟢 Aging: Active (%1s interval)").arg(interval));
        m_statusBadge->setStyleSheet(R"(
            QLabel {
                background-color: #ecfdf5;
                color: #065f46;
                border: 1px solid #a7f3d0;
                border-radius: 6px;
                padding: 6px 12px;
                font-size: 12px;
                font-weight: 600;
            }
        )");
        bool canManage = m_service->currentSession().hasPermission(QStringLiteral("action:ManageScheduling"));
        m_startAgingButton->setEnabled(false);
        m_stopAgingButton->setEnabled(canManage);
    } else {
        m_statusBadge->setText(QStringLiteral("⚪ Aging: Stopped (%1s interval)").arg(interval));
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
        bool canManage = m_service->currentSession().hasPermission(QStringLiteral("action:ManageScheduling"));
        m_startAgingButton->setEnabled(canManage);
        m_stopAgingButton->setEnabled(false);
    }

    // Running process banner
    if (m_service->scheduler() && m_service->scheduler()->hasRunningProcess()) {
        Models::Process runProc = m_service->scheduler()->runningProcess();
        m_runningBanner->setText(QStringLiteral("⚡ Currently RUNNING: Flight %1 (PID: %2, Airline: %3) | Burst: %4s | Eff Priority: %5")
                                     .arg(runProc.flightNumber.isEmpty() ? QString::number(runProc.flightId) : runProc.flightNumber)
                                     .arg(runProc.flightId)
                                     .arg(runProc.airline.isEmpty() ? QStringLiteral("N/A") : runProc.airline)
                                     .arg(runProc.serviceTime)
                                     .arg(runProc.effectivePriority));
        m_runningBanner->setStyleSheet(R"(
            QLabel {
                background-color: #eff6ff;
                border: 1px solid #bfdbfe;
                border-radius: 8px;
                padding: 10px 14px;
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
                padding: 10px 14px;
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
        fnItem->setFont([&]() { QFont f; f.setBold(true); return f; }());
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
            effItem->setFont([&]() { QFont f; f.setBold(true); return f; }());
        } else if (p.effectivePriority < p.basePriority) {
            // Priority improved through aging
            effItem->setForeground(QBrush(QColor(21, 128, 61))); // Green
            effItem->setFont([&]() { QFont f; f.setBold(true); return f; }());
        }
        m_tableWidget->setItem(row, 4, effItem);

        // Col 5: Emergency
        QTableWidgetItem *emItem = new QTableWidgetItem(p.isEmergency ? QStringLiteral("🚨 YES") : QStringLiteral("No"));
        emItem->setTextAlignment(Qt::AlignCenter);
        if (p.isEmergency) {
            emItem->setForeground(QBrush(QColor(220, 38, 38)));
            emItem->setFont([&]() { QFont f; f.setBold(true); return f; }());
        }
        m_tableWidget->setItem(row, 5, emItem);

        // Col 6: Aging Level
        QString agingStr;
        if (p.agingLevel > 0) {
            agingStr = QStringLiteral("+%1 level%2").arg(p.agingLevel).arg(p.agingLevel > 1 ? QStringLiteral("s") : QStringLiteral(""));
        } else {
            agingStr = QStringLiteral("0 (None)");
        }
        QTableWidgetItem *agItem = new QTableWidgetItem(agingStr);
        agItem->setTextAlignment(Qt::AlignCenter);
        if (p.agingLevel > 0) {
            agItem->setForeground(QBrush(QColor(5, 150, 105)));
            agItem->setFont([&]() { QFont f; f.setBold(true); return f; }());
        }
        m_tableWidget->setItem(row, 6, agItem);

        // Col 7: Continuous Wait
        int waitSec = p.readyWaitingDurationSeconds(now);
        QString waitText;
        if (waitSec >= 60) {
            waitText = QStringLiteral("%1m %2s").arg(waitSec / 60).arg(waitSec % 60);
        } else {
            waitText = QStringLiteral("%1s").arg(waitSec);
        }
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
    // Update live waiting durations without rebuilding the entire table
    QDateTime now = QDateTime::currentDateTime();
    for (int row = 0; row < m_cachedProcesses.size() && row < m_tableWidget->rowCount(); ++row) {
        const Models::Process &p = m_cachedProcesses.at(row);
        int waitSec = p.readyWaitingDurationSeconds(now);
        QString waitText;
        if (waitSec >= 60) {
            waitText = QStringLiteral("%1m %2s").arg(waitSec / 60).arg(waitSec % 60);
        } else {
            waitText = QStringLiteral("%1s").arg(waitSec);
        }
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

