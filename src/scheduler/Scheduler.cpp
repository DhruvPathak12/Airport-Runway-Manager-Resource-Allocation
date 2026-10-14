#include "Scheduler.h"

#include <QDebug>

Scheduler::Scheduler(QObject *parent)
    : QObject(parent)
{
}

bool Scheduler::selectNextProcess(Models::Process *outProcess, QString *outReason) const
{
    if (m_readyQueue.isEmpty()) {
        if (outReason) {
            *outReason = QStringLiteral("Ready queue is empty: no processes available to schedule.");
        }
        return false;
    }

    const QList<Models::Process> procs = m_readyQueue.allProcesses();
    int bestIndex = -1;

    for (int i = 0; i < procs.size(); ++i) {
        const Models::Process &candidate = procs.at(i);

        // Filter: Must be in Ready state and valid
        if (candidate.state != Models::ProcessState::Ready) {
            continue;
        }

        QString valErr;
        if (!candidate.validate(&valErr)) {
            continue;
        }

        if (bestIndex == -1) {
            bestIndex = i;
            continue;
        }

        const Models::Process &best = procs.at(bestIndex);

        // 1. Lower numeric effective priority wins (1 = highest, 5 = lowest)
        if (candidate.effectivePriority < best.effectivePriority) {
            bestIndex = i;
        } else if (candidate.effectivePriority == best.effectivePriority) {
            // 2. Tie-breaker 1: Earlier arrivalTime wins
            if (candidate.arrivalTime.isValid() && best.arrivalTime.isValid()) {
                if (candidate.arrivalTime < best.arrivalTime) {
                    bestIndex = i;
                } else if (candidate.arrivalTime == best.arrivalTime) {
                    // 3. Tie-breaker 2: Smaller flightId (stable PID) wins
                    if (candidate.flightId < best.flightId) {
                        bestIndex = i;
                    }
                }
            } else if (candidate.arrivalTime.isValid() && !best.arrivalTime.isValid()) {
                bestIndex = i;
            } else if (!candidate.arrivalTime.isValid() && !best.arrivalTime.isValid()) {
                if (candidate.flightId < best.flightId) {
                    bestIndex = i;
                }
            }
        }
    }

    if (bestIndex == -1) {
        if (outReason) {
            *outReason = QStringLiteral("No eligible READY process found in queue.");
        }
        return false;
    }

    if (outProcess) {
        *outProcess = procs.at(bestIndex);
    }
    return true;
}

Scheduler::DispatchResult Scheduler::dispatchNextProcess()
{
    DispatchResult res;

    // 1. Non-preemptive check: If already managing a RUNNING process, reject new dispatch
    if (m_hasRunningProcess) {
        res.success = false;
        res.process = m_runningProcess;
        res.reason = QStringLiteral("Non-preemptive scheduler busy: process for flight %1 is currently RUNNING.")
                         .arg(m_runningProcess.flightId);
        emit schedulerError(res.reason);
        return res;
    }

    // 2. Select highest-priority candidate
    Models::Process candidate;
    QString selectReason;
    if (!selectNextProcess(&candidate, &selectReason)) {
        res.success = false;
        res.reason = selectReason;
        emit schedulerError(res.reason);
        return res;
    }

    // 3. Validate state transition to RUNNING
    QString transReason;
    if (!candidate.canTransitionTo(Models::ProcessState::Running, &transReason)) {
        res.success = false;
        res.reason = QStringLiteral("Cannot dispatch flight %1: %2").arg(candidate.flightId).arg(transReason);
        emit schedulerError(res.reason);
        return res;
    }

    // 4. Remove candidate from ready queue by flightId (preserves relative order of others)
    Models::Process removed;
    QString removeErr;
    if (!m_readyQueue.removeByFlightId(candidate.flightId, &removed, &removeErr)) {
        res.success = false;
        res.reason = QStringLiteral("Failed to remove flight %1 from ready queue: %2")
                         .arg(candidate.flightId).arg(removeErr);
        emit schedulerError(res.reason);
        return res;
    }

    // 5. Apply state transition to RUNNING
    removed.transitionTo(Models::ProcessState::Running);

    // 6. Record as currently running process
    m_runningProcess = removed;
    m_hasRunningProcess = true;

    res.success = true;
    res.process = removed;
    res.reason = QStringLiteral("Successfully dispatched flight %1 (Priority %2) to RUNNING.")
                     .arg(removed.flightId)
                     .arg(removed.effectivePriority);

    emit processDispatched(m_runningProcess);
    emit schedulingComplete();

    return res;
}

void Scheduler::runScheduler()
{
    // Day 11 Non-Preemptive Priority Scheduler cycle:
    // Dispatches one eligible process from READY to RUNNING.
    dispatchNextProcess();
}

void Scheduler::setRunningProcess(const Models::Process &process)
{
    m_runningProcess = process;
    m_hasRunningProcess = (process.state == Models::ProcessState::Running);
}

void Scheduler::clearRunningProcess()
{
    m_runningProcess = Models::Process();
    m_hasRunningProcess = false;
}

Scheduler::CompletionResult Scheduler::completeRunningProcess(const QDateTime &completedAt)
{
    CompletionResult res;
    if (!m_hasRunningProcess) {
        res.success = false;
        res.reason = QStringLiteral("No process is currently RUNNING to complete.");
        emit schedulerError(res.reason);
        return res;
    }

    res = completeProcess(m_runningProcess, completedAt);
    if (res.success) {
        clearRunningProcess();
    }
    return res;
}

Scheduler::CompletionResult Scheduler::completeProcess(Models::Process process, const QDateTime &completedAt)
{
    CompletionResult res;

    // 1. Prevent duplicate completion records
    for (const auto &cp : m_completedProcesses) {
        if (cp.flightId == process.flightId) {
            res.success = false;
            res.reason = QStringLiteral("Process for flight %1 has already been completed.").arg(process.flightId);
            emit schedulerError(res.reason);
            return res;
        }
    }

    // 2. Validate state transition to Completed
    QString transReason;
    if (!process.canTransitionTo(Models::ProcessState::Completed, &transReason)) {
        res.success = false;
        res.reason = QStringLiteral("Cannot complete process %1: %2").arg(process.flightId).arg(transReason);
        emit schedulerError(res.reason);
        return res;
    }

    // 3. Determine completion timestamp: if invalid, default to arrivalTime + serviceTime
    QDateTime compTime = completedAt.isValid() ? completedAt : process.arrivalTime.addSecs(process.serviceTime);

    // 4. Calculate metrics (CT, TAT, WT)
    QString metricErr;
    if (!process.calculateMetrics(compTime, &metricErr)) {
        res.success = false;
        res.reason = QStringLiteral("Failed to calculate metrics for process %1: %2").arg(process.flightId).arg(metricErr);
        emit schedulerError(res.reason);
        return res;
    }

    // 5. Apply transition to Completed
    process.transitionTo(Models::ProcessState::Completed);

    // 6. Record in completed processes
    m_completedProcesses.append(process);

    res.success = true;
    res.process = process;
    res.reason = QStringLiteral("Successfully completed flight %1 (CT=%2, TAT=%3s, WT=%4s).")
                     .arg(process.flightId)
                     .arg(process.completionTime.toString(Qt::ISODate))
                     .arg(process.turnaroundTime)
                     .arg(process.waitingTime);

    emit processCompleted(process);
    return res;
}

bool Scheduler::blockRunningProcess(Models::Process *outBlocked, QString *outReason)
{
    if (!m_hasRunningProcess) {
        if (outReason) *outReason = QStringLiteral("No process is currently RUNNING to block.");
        return false;
    }

    QString transReason;
    if (!m_runningProcess.canTransitionTo(Models::ProcessState::Blocked, &transReason)) {
        if (outReason) *outReason = QStringLiteral("Cannot block flight %1: %2").arg(m_runningProcess.flightId).arg(transReason);
        return false;
    }

    m_runningProcess.transitionTo(Models::ProcessState::Blocked);
    if (outBlocked) {
        *outBlocked = m_runningProcess;
    }

    clearRunningProcess();
    return true;
}

Scheduler::PerformanceMetrics Scheduler::calculateAggregateMetrics() const
{
    PerformanceMetrics metrics;
    metrics.completedCount = m_completedProcesses.size();

    if (metrics.completedCount == 0) {
        return metrics;
    }

    int totalTat = 0;
    int totalWt = 0;
    int minWt = m_completedProcesses.first().waitingTime;
    int maxWt = m_completedProcesses.first().waitingTime;
    int minTat = m_completedProcesses.first().turnaroundTime;
    int maxTat = m_completedProcesses.first().turnaroundTime;

    for (const auto &p : m_completedProcesses) {
        totalTat += p.turnaroundTime;
        totalWt += p.waitingTime;

        if (p.waitingTime < minWt) minWt = p.waitingTime;
        if (p.waitingTime > maxWt) maxWt = p.waitingTime;
        if (p.turnaroundTime < minTat) minTat = p.turnaroundTime;
        if (p.turnaroundTime > maxTat) maxTat = p.turnaroundTime;
    }

    metrics.totalTurnaroundTime = totalTat;
    metrics.totalWaitingTime = totalWt;
    metrics.averageTurnaroundTime = static_cast<double>(totalTat) / static_cast<double>(metrics.completedCount);
    metrics.averageWaitingTime = static_cast<double>(totalWt) / static_cast<double>(metrics.completedCount);
    metrics.minWaitingTime = minWt;
    metrics.maxWaitingTime = maxWt;
    metrics.minTurnaroundTime = minTat;
    metrics.maxTurnaroundTime = maxTat;

    return metrics;
}

void Scheduler::reset()
{
    m_readyQueue.clear();
    clearRunningProcess();
    m_completedProcesses.clear();
}

