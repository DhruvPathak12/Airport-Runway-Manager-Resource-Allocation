#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QTabWidget>
#include <QTextEdit>

#include "models/Process.h"
#include "scheduler/StarvationSimulation.h"

class ApplicationService;

/**
 * @brief GUI view for monitoring the OS Priority Scheduler, Ready Queue, and Priority Aging,
 * and demonstrating Starvation and Starvation Prevention (Day 18).
 */
class SchedulingView : public QWidget
{
    Q_OBJECT

public:
    explicit SchedulingView(ApplicationService *service, QWidget *parent = nullptr);
    ~SchedulingView() override;

    /**
     * @brief Updates control availability and action permissions based on the active session role.
     */
    void updatePermissions();

public slots:
    /**
     * @brief Refreshes the ready queue and scheduling processes view.
     */
    void refreshView();

private slots:
    // Live Tab slots
    void onStartAgingClicked();
    void onStopAgingClicked();
    void onIntervalChanged(int seconds);
    void onEvaluateAgingClicked();
    void onDispatchClicked();
    void onClockTick();

    // Demonstration Tab slots (Day 18)
    void onRunWithoutAgingClicked();
    void onRunWithAgingClicked();
    void onStepSimulationClicked();
    void onResetSimulationClicked();

private:
    void setupUi();
    QWidget* createLiveTab();
    QWidget* createStarvationTab();

    void renderProcessesTable(const QList<Models::Process> &processes);
    void updateStatusBanner();

    void renderSimulationTable(const SchedulerComponents::SimulationMetrics &metrics);
    void updateComparisonSummary();

    ApplicationService *m_service = nullptr;

    QTabWidget     *m_tabWidget           = nullptr;

    // ── Live Tab controls ────────────────────────────────────────────────────
    QLabel         *m_statusBadge         = nullptr;
    QSpinBox       *m_intervalSpin        = nullptr;
    QPushButton    *m_startAgingButton    = nullptr;
    QPushButton    *m_stopAgingButton     = nullptr;
    QPushButton    *m_evalAgingButton     = nullptr;
    QPushButton    *m_dispatchButton      = nullptr;
    QPushButton    *m_refreshButton       = nullptr;
    QLabel         *m_runningBanner       = nullptr;
    QLabel         *m_permNotice          = nullptr;
    QTableWidget   *m_tableWidget         = nullptr;
    QTimer         *m_clockTimer          = nullptr;

    QList<Models::Process> m_cachedProcesses;

    // ── Starvation Demonstration Tab controls (Day 18) ───────────────────────
    QPushButton    *m_btnRunWithoutAging  = nullptr;
    QPushButton    *m_btnRunWithAging     = nullptr;
    QPushButton    *m_btnStepSim          = nullptr;
    QPushButton    *m_btnResetSim         = nullptr;
    QSpinBox       *m_simAgingIntervalSpin = nullptr;
    QSpinBox       *m_simHorizonSpin      = nullptr;

    QLabel         *m_simStatusBadge      = nullptr;
    QLabel         *m_simClockLabel       = nullptr;
    QLabel         *m_simPermNotice       = nullptr;

    QTableWidget   *m_comparisonTable     = nullptr;
    QLabel         *m_explanationBox      = nullptr;
    QTableWidget   *m_simProcessesTable   = nullptr;

    SchedulerComponents::StarvationSimulation m_interactiveSim;
    SchedulerComponents::SimulationMetrics m_metricsWithoutAging;
    SchedulerComponents::SimulationMetrics m_metricsWithAging;
    bool m_hasRunWithoutAging = false;
    bool m_hasRunWithAging = false;
};
