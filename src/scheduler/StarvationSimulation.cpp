#include "scheduler/StarvationSimulation.h"

#include <algorithm>

namespace SchedulerComponents {

StarvationSimulation::StarvationSimulation(QObject *parent)
    : QObject(parent)
{
    initializeWorkload();
}

StarvationSimulation::StarvationSimulation(const SimulationConfig &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    initializeWorkload();
}

void StarvationSimulation::setConfig(const SimulationConfig &config)
{
    m_config = config;
    reset();
}

QList<Models::Process> StarvationSimulation::createStandardInitialProcesses(const QDateTime &baseTime)
{
    QList<Models::Process> procs;

    // P1: AI-101 | Priority 5 | Arrival 0s | Burst 20s
    Models::Process p1(1, 101, 5, 20, Models::ProcessState::Ready);
    p1.flightNumber = QStringLiteral("AI-101");
    p1.airline = QStringLiteral("Air India");
    p1.arrivalTime = baseTime.addSecs(0);
    p1.readySince = p1.arrivalTime;
    p1.effectivePriority = 5;
    procs.append(p1);

    // P2: 6E-202 | Priority 2 | Arrival 1s | Burst 20s
    Models::Process p2(2, 202, 2, 20, Models::ProcessState::Ready);
    p2.flightNumber = QStringLiteral("6E-202");
    p2.airline = QStringLiteral("IndiGo");
    p2.arrivalTime = baseTime.addSecs(1);
    p2.readySince = p2.arrivalTime;
    p2.effectivePriority = 2;
    procs.append(p2);

    // P3: AI-303 | Priority 1 | Arrival 2s | Burst 20s
    Models::Process p3(3, 303, 1, 20, Models::ProcessState::Ready);
    p3.flightNumber = QStringLiteral("AI-303");
    p3.airline = QStringLiteral("Air India");
    p3.arrivalTime = baseTime.addSecs(2);
    p3.readySince = p3.arrivalTime;
    p3.effectivePriority = 1;
    procs.append(p3);

    // P4: SG-404 | Priority 1 | Arrival 3s | Burst 20s
    Models::Process p4(4, 404, 1, 20, Models::ProcessState::Ready);
    p4.flightNumber = QStringLiteral("SG-404");
    p4.airline = QStringLiteral("SpiceJet");
    p4.arrivalTime = baseTime.addSecs(3);
    p4.readySince = p4.arrivalTime;
    p4.effectivePriority = 1;
    procs.append(p4);

    // P5: UK-505 | Priority 1 | Arrival 4s | Burst 20s
    Models::Process p5(5, 505, 1, 20, Models::ProcessState::Ready);
    p5.flightNumber = QStringLiteral("UK-505");
    p5.airline = QStringLiteral("Vistara");
    p5.arrivalTime = baseTime.addSecs(4);
    p5.readySince = p5.arrivalTime;
    p5.effectivePriority = 1;
    procs.append(p5);

    return procs;
}

QList<Models::Process> StarvationSimulation::createStandardArrivalStream(const QDateTime &baseTime)
{
    QList<Models::Process> stream;

    // Higher-priority arrivals introduced periodically
    // P6: QP-606 | Priority 1 | Arrival 20s | Burst 20s
    Models::Process p6(6, 606, 1, 20, Models::ProcessState::Ready);
    p6.flightNumber = QStringLiteral("QP-606");
    p6.airline = QStringLiteral("Akasa Air");
    p6.arrivalTime = baseTime.addSecs(20);
    p6.readySince = p6.arrivalTime;
    p6.effectivePriority = 1;
    stream.append(p6);

    // P7: IX-707 | Priority 2 | Arrival 40s | Burst 20s
    Models::Process p7(7, 707, 2, 20, Models::ProcessState::Ready);
    p7.flightNumber = QStringLiteral("IX-707");
    p7.airline = QStringLiteral("Air India Express");
    p7.arrivalTime = baseTime.addSecs(40);
    p7.readySince = p7.arrivalTime;
    p7.effectivePriority = 2;
    stream.append(p7);

    // P8: G8-808 | Priority 1 | Arrival 60s | Burst 20s
    Models::Process p8(8, 808, 1, 20, Models::ProcessState::Ready);
    p8.flightNumber = QStringLiteral("G8-808");
    p8.airline = QStringLiteral("Go First");
    p8.arrivalTime = baseTime.addSecs(60);
    p8.readySince = p8.arrivalTime;
    p8.effectivePriority = 1;
    stream.append(p8);

    // P9: I5-909 | Priority 2 | Arrival 80s | Burst 20s
    Models::Process p9(9, 909, 2, 20, Models::ProcessState::Ready);
    p9.flightNumber = QStringLiteral("I5-909");
    p9.airline = QStringLiteral("AIX Connect");
    p9.arrivalTime = baseTime.addSecs(80);
    p9.readySince = p9.arrivalTime;
    p9.effectivePriority = 2;
    stream.append(p9);

    // P10: SG-1010 | Priority 1 | Arrival 100s | Burst 20s
    Models::Process p10(10, 1010, 1, 20, Models::ProcessState::Ready);
    p10.flightNumber = QStringLiteral("SG-1010");
    p10.airline = QStringLiteral("SpiceJet");
    p10.arrivalTime = baseTime.addSecs(100);
    p10.readySince = p10.arrivalTime;
    p10.effectivePriority = 1;
    stream.append(p10);

    // P11: AI-1111 | Priority 1 | Arrival 120s | Burst 20s
    Models::Process p11(11, 1111, 1, 20, Models::ProcessState::Ready);
    p11.flightNumber = QStringLiteral("AI-1111");
    p11.airline = QStringLiteral("Air India");
    p11.arrivalTime = baseTime.addSecs(120);
    p11.readySince = p11.arrivalTime;
    p11.effectivePriority = 1;
    stream.append(p11);

    // P12: 6E-1212 | Priority 2 | Arrival 140s | Burst 20s
    Models::Process p12(12, 1212, 2, 20, Models::ProcessState::Ready);
    p12.flightNumber = QStringLiteral("6E-1212");
    p12.airline = QStringLiteral("IndiGo");
    p12.arrivalTime = baseTime.addSecs(140);
    p12.readySince = p12.arrivalTime;
    p12.effectivePriority = 2;
    stream.append(p12);

    return stream;
}

void StarvationSimulation::initializeWorkload()
{
    m_currentTimeSec = 0;
    m_isRunningProcess = false;
    m_runningProcess = Models::Process();
    m_runningProcessCompletionTimeSec = 0;

    m_readyQueue.clear();
    m_dispatchHistory.clear();
    m_completedProcesses.clear();

    m_allIntroduced = createStandardInitialProcesses(m_config.baseTime);
    QList<Models::Process> stream = createStandardArrivalStream(m_config.baseTime);
    m_allIntroduced.append(stream);

    // Pending arrivals sorted by arrival time
    m_pendingArrivals = m_allIntroduced;
    std::sort(m_pendingArrivals.begin(), m_pendingArrivals.end(), [this](const Models::Process &a, const Models::Process &b) {
        return m_config.baseTime.secsTo(a.arrivalTime) < m_config.baseTime.secsTo(b.arrivalTime);
    });
}

void StarvationSimulation::reset()
{
    initializeWorkload();
}

bool StarvationSimulation::isFinished() const
{
    return m_currentTimeSec >= m_config.simulationHorizonSeconds;
}

void StarvationSimulation::applyAgingIfEnabled()
{
    if (m_config.mode != SimulationMode::WithAging) {
        return;
    }

    QDateTime currentSimTime = m_config.baseTime.addSecs(m_currentTimeSec);
    QList<Models::Process> procs = m_readyQueue.allProcesses();
    bool anyChanged = false;

    for (auto &p : procs) {
        if (p.state == Models::ProcessState::Ready) {
            if (p.applyAging(currentSimTime, m_config.agingIntervalSeconds)) {
                anyChanged = true;
            }
        }
    }

    if (anyChanged) {
        m_readyQueue.clear();
        for (const auto &p : procs) {
            m_readyQueue.enqueue(p);
        }
        m_readyQueue.reorder();
    }
}

bool StarvationSimulation::selectNextCandidate(Models::Process *outCandidate) const
{
    QList<Models::Process> procs = m_readyQueue.allProcesses();
    int bestIndex = -1;

    for (int i = 0; i < procs.size(); ++i) {
        const auto &candidate = procs.at(i);
        if (candidate.state != Models::ProcessState::Ready) {
            continue;
        }

        if (bestIndex == -1) {
            bestIndex = i;
            continue;
        }

        const auto &best = procs.at(bestIndex);

        // 1. Lowest numerical effective priority wins
        if (candidate.effectivePriority < best.effectivePriority) {
            bestIndex = i;
        } else if (candidate.effectivePriority == best.effectivePriority) {
            // 2. Tie-breaker 1: Earlier arrivalTime wins
            if (candidate.arrivalTime < best.arrivalTime) {
                bestIndex = i;
            } else if (candidate.arrivalTime == best.arrivalTime) {
                // 3. Tie-breaker 2: Smaller flightId wins
                if (candidate.flightId < best.flightId) {
                    bestIndex = i;
                }
            }
        }
    }

    if (bestIndex == -1) {
        return false;
    }

    if (outCandidate) {
        *outCandidate = procs.at(bestIndex);
    }
    return true;
}

bool StarvationSimulation::step()
{
    if (isFinished()) {
        return false;
    }

    // 1. Check if running process finishes at or before m_currentTimeSec
    if (m_isRunningProcess && m_currentTimeSec >= m_runningProcessCompletionTimeSec) {
        m_runningProcess.transitionTo(Models::ProcessState::Completed);
        m_runningProcess.completionTime = m_config.baseTime.addSecs(m_runningProcessCompletionTimeSec);
        m_completedProcesses.append(m_runningProcess);
        m_isRunningProcess = false;
    }

    // 2. Enqueue any newly arrived processes up to m_currentTimeSec
    while (!m_pendingArrivals.isEmpty()) {
        int arrSec = m_config.baseTime.secsTo(m_pendingArrivals.first().arrivalTime);
        if (arrSec <= m_currentTimeSec) {
            Models::Process arriving = m_pendingArrivals.takeFirst();
            arriving.readySince = arriving.arrivalTime;
            arriving.state = Models::ProcessState::Ready;
            m_readyQueue.enqueue(arriving);
        } else {
            break;
        }
    }

    // 3. Apply aging to waiting READY processes if enabled
    applyAgingIfEnabled();

    // 4. Dispatch candidate if CPU / runway is idle and initial batch has arrived (t >= 4s)
    if (!m_isRunningProcess && m_currentTimeSec >= 4) {
        Models::Process candidate;
        if (selectNextCandidate(&candidate)) {
            // Remove from ready queue
            m_readyQueue.removeByFlightId(candidate.flightId);
            candidate.transitionTo(Models::ProcessState::Running);

            int arrSec = m_config.baseTime.secsTo(candidate.arrivalTime);
            int waitSec = m_currentTimeSec - arrSec;
            if (waitSec < 0) waitSec = 0;

            candidate.waitingTime = waitSec;
            m_runningProcess = candidate;
            m_isRunningProcess = true;
            m_runningProcessCompletionTimeSec = m_currentTimeSec + candidate.serviceTime;

            DispatchEvent ev;
            ev.flightId = candidate.flightId;
            ev.flightNumber = candidate.flightNumber;
            ev.basePriority = candidate.basePriority;
            ev.effectivePriority = candidate.effectivePriority;
            ev.agingLevel = candidate.agingLevel;
            ev.dispatchTimeSec = m_currentTimeSec;
            ev.burstTimeSec = candidate.serviceTime;
            ev.completionTimeSec = m_runningProcessCompletionTimeSec;
            ev.waitingTimeSec = waitSec;
            m_dispatchHistory.append(ev);
        }
    }

    // 5. Advance to next event timestamp
    int nextEventSec = m_config.simulationHorizonSeconds;

    // Potential event 1: next pending arrival
    if (!m_pendingArrivals.isEmpty()) {
        int nextArrSec = m_config.baseTime.secsTo(m_pendingArrivals.first().arrivalTime);
        if (nextArrSec > m_currentTimeSec && nextArrSec < nextEventSec) {
            nextEventSec = nextArrSec;
        }
    }

    // Potential event 2: running process completion
    if (m_isRunningProcess && m_runningProcessCompletionTimeSec > m_currentTimeSec && m_runningProcessCompletionTimeSec < nextEventSec) {
        nextEventSec = m_runningProcessCompletionTimeSec;
    }

    // Potential event 3: initial dispatch trigger at t = 4s
    if (m_currentTimeSec < 4 && 4 < nextEventSec) {
        nextEventSec = 4;
    }

    if (nextEventSec > m_currentTimeSec) {
        m_currentTimeSec = nextEventSec;
    } else {
        // If no events remaining before horizon, jump to horizon
        m_currentTimeSec = m_config.simulationHorizonSeconds;
    }

    // Re-check completion at new timestamp
    if (m_isRunningProcess && m_currentTimeSec >= m_runningProcessCompletionTimeSec) {
        m_runningProcess.transitionTo(Models::ProcessState::Completed);
        m_runningProcess.completionTime = m_config.baseTime.addSecs(m_runningProcessCompletionTimeSec);
        m_completedProcesses.append(m_runningProcess);
        m_isRunningProcess = false;
    }

    // Enqueue any arrivals at this new timestamp
    while (!m_pendingArrivals.isEmpty()) {
        int arrSec = m_config.baseTime.secsTo(m_pendingArrivals.first().arrivalTime);
        if (arrSec <= m_currentTimeSec) {
            Models::Process arriving = m_pendingArrivals.takeFirst();
            arriving.readySince = arriving.arrivalTime;
            arriving.state = Models::ProcessState::Ready;
            m_readyQueue.enqueue(arriving);
        } else {
            break;
        }
    }

    applyAgingIfEnabled();

    return !isFinished();
}

SimulationMetrics StarvationSimulation::runToHorizon()
{
    while (step()) {
        if (m_currentTimeSec >= m_config.simulationHorizonSeconds) {
            break;
        }
    }
    return currentMetrics();
}

SimulationMetrics StarvationSimulation::currentMetrics() const
{
    SimulationMetrics m;
    m.mode = m_config.mode;
    m.totalIntroduced = m_allIntroduced.size();
    m.totalDispatched = m_dispatchHistory.size();
    m.dispatchEvents = m_dispatchHistory;
    m.simulationTimeSec = m_currentTimeSec;
    m.lowPriorityFlightId = 101;
    m.lowPriorityBasePriority = 5;

    for (const auto &ev : m_dispatchHistory) {
        m.dispatchOrder.append(QStringLiteral("%1 (P%2, wait %3s)")
                                   .arg(ev.flightNumber)
                                   .arg(ev.effectivePriority)
                                   .arg(ev.waitingTimeSec));
        m.waitingTimesByFlightId[ev.flightId] = ev.waitingTimeSec;
    }

    int higherDispatches = 0;
    bool lowDispatched = false;
    int lowWait = 0;
    int lowFinalEff = 5;

    for (const auto &ev : m_dispatchHistory) {
        if (ev.flightId == m.lowPriorityFlightId) {
            lowDispatched = true;
            lowWait = ev.waitingTimeSec;
            lowFinalEff = ev.effectivePriority;
            break;
        } else {
            higherDispatches++;
        }
    }

    if (!lowDispatched) {
        // Low priority process was not dispatched within horizon
        // Determine waiting duration up to current simulated time
        lowWait = m_currentTimeSec - 0; // P1 arrived at 0s
        if (lowWait < 0) lowWait = 0;

        // Check if P1 is still in ready queue
        const Models::Process *p1InQueue = m_readyQueue.findByFlightId(m.lowPriorityFlightId);
        if (p1InQueue) {
            lowFinalEff = p1InQueue->effectivePriority;
        } else {
            lowFinalEff = 5;
        }
    }

    m.lowPriorityDispatched = lowDispatched;
    m.lowPriorityWaitingTime = lowWait;
    m.higherPriorityDispatchesBeforeLow = higherDispatches;
    m.lowPriorityFinalEffectivePriority = lowFinalEff;

    // Starvation definition in controlled workload:
    // Low priority process remains un-dispatched while higher-priority arrivals continuously receive scheduling preference.
    m.starvationObserved = (!lowDispatched && higherDispatches >= 3);

    // Compute max ready waiting duration across all introduced processes
    int maxWait = 0;
    for (const auto &ev : m_dispatchHistory) {
        if (ev.waitingTimeSec > maxWait) {
            maxWait = ev.waitingTimeSec;
        }
    }
    if (!lowDispatched && lowWait > maxWait) {
        maxWait = lowWait;
    }
    m.maxReadyWaitingDuration = maxWait;

    // Build process snapshots for UI and inspection
    for (const auto &orig : m_allIntroduced) {
        ProcessSnapshot snap;
        snap.flightId = orig.flightId;
        snap.flightNumber = orig.flightNumber;
        snap.airline = orig.airline;
        snap.basePriority = orig.basePriority;
        snap.arrivalTimeSec = m_config.baseTime.secsTo(orig.arrivalTime);

        // Find in dispatch history
        bool dispatched = false;
        for (const auto &ev : m_dispatchHistory) {
            if (ev.flightId == orig.flightId) {
                dispatched = true;
                snap.isDispatched = true;
                snap.dispatchTimeSec = ev.dispatchTimeSec;
                snap.waitingDurationSec = ev.waitingTimeSec;
                snap.effectivePriority = ev.effectivePriority;
                snap.agingLevel = ev.agingLevel;
                if (m_isRunningProcess && m_runningProcess.flightId == orig.flightId) {
                    snap.stateString = QStringLiteral("Running");
                } else {
                    snap.stateString = QStringLiteral("Completed");
                }
                break;
            }
        }

        if (!dispatched) {
            snap.isDispatched = false;
            snap.dispatchTimeSec = -1;
            const Models::Process *inQueue = m_readyQueue.findByFlightId(orig.flightId);
            if (inQueue) {
                snap.stateString = QStringLiteral("Ready");
                snap.effectivePriority = inQueue->effectivePriority;
                snap.agingLevel = inQueue->agingLevel;
                snap.waitingDurationSec = inQueue->readyWaitingDurationSeconds(m_config.baseTime.addSecs(m_currentTimeSec));
            } else {
                // Pending arrival or not yet entered
                snap.stateString = QStringLiteral("Pending");
                snap.effectivePriority = orig.basePriority;
                snap.agingLevel = 0;
                snap.waitingDurationSec = 0;
            }
        }

        m.processSnapshots.append(snap);
    }

    return m;
}

SimulationMetrics StarvationSimulation::runStandardSimulation(SimulationMode mode, int agingInterval, int horizon)
{
    SimulationConfig config;
    config.mode = mode;
    config.agingIntervalSeconds = agingInterval;
    config.simulationHorizonSeconds = horizon;

    StarvationSimulation sim(config);
    return sim.runToHorizon();
}

} // namespace SchedulerComponents
