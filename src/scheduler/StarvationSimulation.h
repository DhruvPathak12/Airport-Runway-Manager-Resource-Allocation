#pragma once

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>

#include "models/Process.h"
#include "scheduler/ReadyQueue.h"

namespace SchedulerComponents {

/**
 * @brief Simulation modes for comparing scheduling behavior with and without aging.
 */
enum class SimulationMode {
    WithoutAging,
    WithAging
};

/**
 * @brief Configuration parameters for a repeatable starvation simulation run.
 */
struct SimulationConfig {
    SimulationMode mode = SimulationMode::WithoutAging;
    int agingIntervalSeconds = 30;
    int simulationHorizonSeconds = 180;
    QDateTime baseTime = QDateTime(QDate(2026, 1, 1), QTime(0, 0, 0));

    bool isValid(QString *outError = nullptr) const {
        if (agingIntervalSeconds <= 0) {
            if (outError) *outError = QStringLiteral("Aging interval must be greater than 0.");
            return false;
        }
        if (simulationHorizonSeconds <= 0) {
            if (outError) *outError = QStringLiteral("Simulation horizon must be greater than 0.");
            return false;
        }
        return true;
    }
};

/**
 * @brief Detailed log event recording a process dispatch.
 */
struct DispatchEvent {
    int flightId = 0;
    QString flightNumber;
    int basePriority = 0;
    int effectivePriority = 0;
    int agingLevel = 0;
    int dispatchTimeSec = 0;
    int burstTimeSec = 0;
    int completionTimeSec = 0;
    int waitingTimeSec = 0;
};

/**
 * @brief Point-in-time snapshot of a simulated process.
 */
struct ProcessSnapshot {
    int flightId = 0;
    QString flightNumber;
    QString airline;
    int basePriority = 0;
    int effectivePriority = 0;
    int agingLevel = 0;
    int arrivalTimeSec = 0;
    int waitingDurationSec = 0;
    QString stateString; // "Ready", "Running", "Completed"
    bool isDispatched = false;
    int dispatchTimeSec = -1;
};

/**
 * @brief Aggregated measurable metrics produced by a simulation run.
 */
struct SimulationMetrics {
    SimulationMode mode = SimulationMode::WithoutAging;
    int totalIntroduced = 0;
    int totalDispatched = 0;
    QList<DispatchEvent> dispatchEvents;
    QStringList dispatchOrder;
    QMap<int, int> waitingTimesByFlightId;
    int lowPriorityFlightId = 101; // P1 AI-101
    int lowPriorityBasePriority = 5;
    int lowPriorityWaitingTime = 0;
    int higherPriorityDispatchesBeforeLow = 0;
    int maxReadyWaitingDuration = 0;
    bool lowPriorityDispatched = false;
    int lowPriorityFinalEffectivePriority = 5;
    bool starvationObserved = false;
    int simulationTimeSec = 0;
    QList<ProcessSnapshot> processSnapshots;

    QString statusDescription() const {
        if (lowPriorityDispatched) {
            return QStringLiteral("Starvation Mitigated (Dispatched at t=%1s)").arg(lowPriorityWaitingTime);
        } else if (starvationObserved) {
            return QStringLiteral("Starvation Observed (Postponed by %1 high-priority dispatches)").arg(higherPriorityDispatchesBeforeLow);
        } else {
            return QStringLiteral("Horizon Reached (%1s)").arg(simulationTimeSec);
        }
    }
};

/**
 * @brief Deterministic simulation engine demonstrating starvation and starvation prevention via priority aging.
 *
 * ── OS Concept Mapping ────────────────────────────────────────────────────────
 *   - Demonstrates indefinite postponement (starvation) in non-preemptive priority scheduling
 *     when higher-priority processes continually arrive.
 *   - Verifies priority aging as the canonical OS starvation prevention mechanism:
 *     eligible processes waiting in the READY queue dynamically improve effective priority
 *     per completed interval until they compete successfully for CPU dispatch.
 *   - Reuses Day 17 Process::applyAging and ReadyQueue tie-breaking rules directly to
 *     guarantee formula parity with production scheduling.
 */
class StarvationSimulation : public QObject
{
    Q_OBJECT

public:
    explicit StarvationSimulation(QObject *parent = nullptr);
    explicit StarvationSimulation(const SimulationConfig &config, QObject *parent = nullptr);
    ~StarvationSimulation() override = default;

    void setConfig(const SimulationConfig &config);
    SimulationConfig config() const { return m_config; }

    void reset();
    bool step();
    SimulationMetrics runToHorizon();

    bool isFinished() const;
    int currentSimulatedTimeSec() const { return m_currentTimeSec; }
    SimulationMetrics currentMetrics() const;

    static SimulationMetrics runStandardSimulation(SimulationMode mode, int agingInterval = 30, int horizon = 180);

    // Initial workload constructors
    static QList<Models::Process> createStandardInitialProcesses(const QDateTime &baseTime);
    static QList<Models::Process> createStandardArrivalStream(const QDateTime &baseTime);

private:
    void initializeWorkload();
    void applyAgingIfEnabled();
    bool selectNextCandidate(Models::Process *outCandidate) const;

    SimulationConfig m_config;
    int m_currentTimeSec = 0;
    bool m_isRunningProcess = false;
    Models::Process m_runningProcess;
    int m_runningProcessCompletionTimeSec = 0;

    QList<Models::Process> m_pendingArrivals;
    ReadyQueue m_readyQueue;
    QList<Models::Process> m_allIntroduced;
    QList<DispatchEvent> m_dispatchHistory;
    QList<Models::Process> m_completedProcesses;
};

} // namespace SchedulerComponents
