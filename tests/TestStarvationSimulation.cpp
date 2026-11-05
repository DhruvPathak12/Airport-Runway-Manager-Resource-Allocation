#include <QTest>
#include <QObject>
#include <QDateTime>
#include <QList>

#include "models/Process.h"
#include "scheduler/StarvationSimulation.h"
#include "scheduler/ReadyQueue.h"
#include "scheduler/Scheduler.h"
#include "scheduler/AgingEngine.h"
#include "services/ApplicationService.h"

using namespace SchedulerComponents;

class TestStarvationSimulation : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ── Simulation correctness (1–8) ─────────────────────────────────────────
    void testSimulationStartsSuccessfully();
    void testSimulationResetRestoresInitialState();
    void testProcessArrivalsOccurAtSimulatedTimes();
    void testSameInitialWorkloadInBothModes();
    void testDispatchOrderFollowsPriorityRules();
    void testDeterministicTieBreakingPreserved();
    void testBurstServiceTimeAdvancesSimulatedTime();
    void testNoProcessDispatchedMoreThanOnce();

    // ── Aging disabled (9–14) ────────────────────────────────────────────────
    void testAgingDisabledDoesNotModifyEffectivePriority();
    void testLowPriorityRemainsReadyWhileHigherPriorityDispatched();
    void testWaitingDurationCalculatedCorrectly();
    void testHigherPriorityDispatchCountCorrect();
    void testSimulationHorizonRespected();
    void testStarvationReportedOnlyUnderActualConditions();

    // ── Aging enabled (15–20) ────────────────────────────────────────────────
    void testEffectivePriorityImprovesAccordingToElapsedReadyWaiting();
    void testBasePriorityRemainsUnchanged();
    void testEmergencyPriorityRemains1();
    void testLowPriorityProcessEventuallyDispatchedInControlledWorkload();
    void testAgingEnabledWaitingTimeCalculatedCorrectly();
    void testAgingEnabledDiffersFromAgingDisabled();

    // ── Integration and regression (21–26) ───────────────────────────────────
    void testExistingPriorityAgingTestsContinuePassing();
    void testExistingEmergencyPriorityIntact();
    void testExistingSchedulerIntact();
    void testNonPreemptiveSchedulingRemainsIntact();
    void testInvalidSimulationConfigurationRejected();
    void testRepeatedSimulationRunsProduceIdenticalResults();
};

void TestStarvationSimulation::initTestCase()
{
}

void TestStarvationSimulation::cleanupTestCase()
{
}

// ── 1. Simulation starts successfully
void TestStarvationSimulation::testSimulationStartsSuccessfully()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithoutAging;
    config.agingIntervalSeconds = 30;
    config.simulationHorizonSeconds = 180;
    QVERIFY(config.isValid());

    StarvationSimulation sim(config);
    QCOMPARE(sim.currentSimulatedTimeSec(), 0);
    QVERIFY(!sim.isFinished());

    SimulationMetrics m = sim.currentMetrics();
    QCOMPARE(m.totalDispatched, 0);
    QVERIFY(m.totalIntroduced > 0);
}

// ── 2. Simulation reset restores initial state
void TestStarvationSimulation::testSimulationResetRestoresInitialState()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithAging;
    config.agingIntervalSeconds = 30;
    config.simulationHorizonSeconds = 180;

    StarvationSimulation sim(config);
    sim.step();
    sim.step();
    QVERIFY(sim.currentSimulatedTimeSec() > 0);

    sim.reset();
    QCOMPARE(sim.currentSimulatedTimeSec(), 0);
    QCOMPARE(sim.currentMetrics().totalDispatched, 0);
    QVERIFY(!sim.isFinished());
}

// ── 3. Process arrivals occur at configured simulated times
void TestStarvationSimulation::testProcessArrivalsOccurAtSimulatedTimes()
{
    QDateTime baseTime(QDate(2026, 1, 1), QTime(0, 0, 0));
    auto init = StarvationSimulation::createStandardInitialProcesses(baseTime);
    QCOMPARE(init.size(), 5);

    QCOMPARE(baseTime.secsTo(init[0].arrivalTime), 0); // P1
    QCOMPARE(baseTime.secsTo(init[1].arrivalTime), 1); // P2
    QCOMPARE(baseTime.secsTo(init[2].arrivalTime), 2); // P3
    QCOMPARE(baseTime.secsTo(init[3].arrivalTime), 3); // P4
    QCOMPARE(baseTime.secsTo(init[4].arrivalTime), 4); // P5

    auto stream = StarvationSimulation::createStandardArrivalStream(baseTime);
    QCOMPARE(stream.size(), 7);
    QCOMPARE(baseTime.secsTo(stream[0].arrivalTime), 20); // P6
    QCOMPARE(baseTime.secsTo(stream[1].arrivalTime), 40); // P7
    QCOMPARE(baseTime.secsTo(stream[2].arrivalTime), 60); // P8
}

// ── 4. The same initial workload is used in both modes
void TestStarvationSimulation::testSameInitialWorkloadInBothModes()
{
    QDateTime baseTime(QDate(2026, 1, 1), QTime(0, 0, 0));
    SimulationConfig cfgA;
    cfgA.mode = SimulationMode::WithoutAging;
    cfgA.baseTime = baseTime;

    SimulationConfig cfgB;
    cfgB.mode = SimulationMode::WithAging;
    cfgB.baseTime = baseTime;

    StarvationSimulation simA(cfgA);
    StarvationSimulation simB(cfgB);

    auto metricsA = simA.currentMetrics();
    auto metricsB = simB.currentMetrics();

    QCOMPARE(metricsA.totalIntroduced, metricsB.totalIntroduced);
    QCOMPARE(metricsA.processSnapshots.size(), metricsB.processSnapshots.size());

    for (int i = 0; i < metricsA.processSnapshots.size(); ++i) {
        QCOMPARE(metricsA.processSnapshots[i].flightId, metricsB.processSnapshots[i].flightId);
        QCOMPARE(metricsA.processSnapshots[i].basePriority, metricsB.processSnapshots[i].basePriority);
        QCOMPARE(metricsA.processSnapshots[i].arrivalTimeSec, metricsB.processSnapshots[i].arrivalTimeSec);
    }
}

// ── 5. Dispatch order follows priority rules
void TestStarvationSimulation::testDispatchOrderFollowsPriorityRules()
{
    // Mode A: High priorities (P3, P4, P5 with priority 1) must be dispatched before P2 (pri 2) and P1 (pri 5)
    SimulationConfig config;
    config.mode = SimulationMode::WithoutAging;
    config.simulationHorizonSeconds = 50;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    QVERIFY(m.dispatchEvents.size() >= 2);
    // First dispatched must be P3 (priority 1)
    QCOMPARE(m.dispatchEvents[0].flightId, 303);
    QCOMPARE(m.dispatchEvents[0].effectivePriority, 1);
    // Second dispatched must be P4 (priority 1)
    QCOMPARE(m.dispatchEvents[1].flightId, 404);
    QCOMPARE(m.dispatchEvents[1].effectivePriority, 1);
}

// ── 6. Deterministic tie-breaking is preserved
void TestStarvationSimulation::testDeterministicTieBreakingPreserved()
{
    // P3 (arr 2s), P4 (arr 3s), P5 (arr 4s) all have priority 1.
    // Earlier arrival time must win!
    SimulationConfig config;
    config.mode = SimulationMode::WithoutAging;
    config.simulationHorizonSeconds = 70;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    QVERIFY(m.dispatchEvents.size() >= 3);
    QCOMPARE(m.dispatchEvents[0].flightId, 303); // arrived at 2s
    QCOMPARE(m.dispatchEvents[1].flightId, 404); // arrived at 3s
    QCOMPARE(m.dispatchEvents[2].flightId, 505); // arrived at 4s
}

// ── 7. Burst/service time advances simulated time correctly
void TestStarvationSimulation::testBurstServiceTimeAdvancesSimulatedTime()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithoutAging;
    config.simulationHorizonSeconds = 180;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    QVERIFY(!m.dispatchEvents.isEmpty());
    for (const auto &ev : m.dispatchEvents) {
        QCOMPARE(ev.completionTimeSec - ev.dispatchTimeSec, ev.burstTimeSec);
    }
}

// ── 8. No process is dispatched more than once
void TestStarvationSimulation::testNoProcessDispatchedMoreThanOnce()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithAging;
    config.simulationHorizonSeconds = 180;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    QSet<int> seen;
    for (const auto &ev : m.dispatchEvents) {
        QVERIFY(!seen.contains(ev.flightId));
        seen.insert(ev.flightId);
    }
}

// ── 9. Aging does not modify effective priority in Mode A
void TestStarvationSimulation::testAgingDisabledDoesNotModifyEffectivePriority()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithoutAging;
    config.simulationHorizonSeconds = 180;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    QCOMPARE(m.lowPriorityFinalEffectivePriority, 5);
    QCOMPARE(m.lowPriorityBasePriority, 5);
}

// ── 10. Low-priority process remains READY while higher-priority work is dispatched
void TestStarvationSimulation::testLowPriorityRemainsReadyWhileHigherPriorityDispatched()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithoutAging;
    config.simulationHorizonSeconds = 180;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    QVERIFY(!m.lowPriorityDispatched);
    QVERIFY(m.higherPriorityDispatchesBeforeLow >= 5);
    QCOMPARE(m.lowPriorityFlightId, 101);
}

// ── 11. Waiting duration is calculated correctly
void TestStarvationSimulation::testWaitingDurationCalculatedCorrectly()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithoutAging;
    config.simulationHorizonSeconds = 100;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    // In Mode A at horizon=100s, P1 arrived at 0s and was not dispatched: wait is 100s
    QCOMPARE(m.lowPriorityWaitingTime, 100);
}

// ── 12. Higher-priority dispatch count is correct
void TestStarvationSimulation::testHigherPriorityDispatchCountCorrect()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithoutAging;
    config.simulationHorizonSeconds = 180;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    QCOMPARE(m.higherPriorityDispatchesBeforeLow, m.totalDispatched);
    QVERIFY(m.higherPriorityDispatchesBeforeLow >= 7);
}

// ── 13. Simulation horizon is respected
void TestStarvationSimulation::testSimulationHorizonRespected()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithoutAging;
    config.simulationHorizonSeconds = 90;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    QCOMPARE(m.simulationTimeSec, 90);
}

// ── 14. Starvation is reported only according to actual observed conditions
void TestStarvationSimulation::testStarvationReportedOnlyUnderActualConditions()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithoutAging;
    config.simulationHorizonSeconds = 180;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    QVERIFY(m.starvationObserved);
    QVERIFY(!m.lowPriorityDispatched);
}

// ── 15. Effective priority improves according to elapsed READY waiting time
void TestStarvationSimulation::testEffectivePriorityImprovesAccordingToElapsedReadyWaiting()
{
    QDateTime base(QDate(2026, 1, 1), QTime(0, 0, 0));
    Models::Process p(1, 101, 5, 20, Models::ProcessState::Ready);
    p.arrivalTime = base;
    p.readySince = base;

    // 0 intervals: eff 5
    QCOMPARE(p.calculateEffectivePriority(base.addSecs(10), 30), 5);
    // 1 interval (30s): eff 4
    QCOMPARE(p.calculateEffectivePriority(base.addSecs(30), 30), 4);
    // 2 intervals (60s): eff 3
    QCOMPARE(p.calculateEffectivePriority(base.addSecs(60), 30), 3);
    // 3 intervals (90s): eff 2
    QCOMPARE(p.calculateEffectivePriority(base.addSecs(90), 30), 2);
    // 4 intervals (120s): eff 1
    QCOMPARE(p.calculateEffectivePriority(base.addSecs(120), 30), 1);
    // 5 intervals (150s): capped at 1
    QCOMPARE(p.calculateEffectivePriority(base.addSecs(150), 30), 1);
}

// ── 16. Base priority remains unchanged
void TestStarvationSimulation::testBasePriorityRemainsUnchanged()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithAging;
    config.simulationHorizonSeconds = 180;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    QCOMPARE(m.lowPriorityBasePriority, 5);
}

// ── 17. Emergency priority remains 1
void TestStarvationSimulation::testEmergencyPriorityRemains1()
{
    QDateTime base(QDate(2026, 1, 1), QTime(0, 0, 0));
    Models::Process p(1, 101, 5, 20, Models::ProcessState::Ready);
    p.arrivalTime = base;
    p.readySince = base;
    p.setEmergency(true);

    QCOMPARE(p.effectivePriority, 1);
    QCOMPARE(p.calculateEffectivePriority(base.addSecs(120), 30), 1);

    // Clearing emergency restores aged priority
    p.applyAging(base.addSecs(60), 30); // 2 intervals -> aged pri 3
    p.setEmergency(false);
    QCOMPARE(p.effectivePriority, 3);
    QCOMPARE(p.basePriority, 5);
}

// ── 18. Low-priority process eventually receives dispatch in the designed controlled workload
void TestStarvationSimulation::testLowPriorityProcessEventuallyDispatchedInControlledWorkload()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithAging;
    config.agingIntervalSeconds = 30;
    config.simulationHorizonSeconds = 180;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    QVERIFY(m.lowPriorityDispatched);
    QCOMPARE(m.lowPriorityFinalEffectivePriority, 1);
    QVERIFY(!m.starvationObserved);
}

// ── 19. Waiting time is calculated correctly in Mode B
void TestStarvationSimulation::testAgingEnabledWaitingTimeCalculatedCorrectly()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithAging;
    config.agingIntervalSeconds = 30;
    config.simulationHorizonSeconds = 180;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();

    QVERIFY(m.lowPriorityWaitingTime > 0);
    // In our deterministic simulation, P1 is dispatched at t=124s
    QCOMPARE(m.lowPriorityWaitingTime, 124);
}

// ── 20. The aging-enabled result differs from aging-disabled where expected
void TestStarvationSimulation::testAgingEnabledDiffersFromAgingDisabled()
{
    SimulationMetrics mA = StarvationSimulation::runStandardSimulation(SimulationMode::WithoutAging, 30, 180);
    SimulationMetrics mB = StarvationSimulation::runStandardSimulation(SimulationMode::WithAging, 30, 180);

    // In Mode A: Not dispatched, starved
    QVERIFY(!mA.lowPriorityDispatched);
    QVERIFY(mA.starvationObserved);
    QCOMPARE(mA.lowPriorityFinalEffectivePriority, 5);

    // In Mode B: Dispatched, mitigated
    QVERIFY(mB.lowPriorityDispatched);
    QVERIFY(!mB.starvationObserved);
    QCOMPARE(mB.lowPriorityFinalEffectivePriority, 1);

    // Waiting times and final effective priorities differ
    QVERIFY(mA.lowPriorityFinalEffectivePriority != mB.lowPriorityFinalEffectivePriority);
    QVERIFY(mA.lowPriorityDispatched != mB.lowPriorityDispatched);
}

// ── 21. Existing Priority Aging tests continue passing
void TestStarvationSimulation::testExistingPriorityAgingTestsContinuePassing()
{
    ReadyQueue queue;
    QDateTime base(QDate(2026, 1, 1), QTime(0, 0, 0));

    Models::Process p1(1, 101, 5, 20, Models::ProcessState::Ready);
    p1.arrivalTime = base;
    p1.readySince = base;
    queue.enqueue(p1);

    AgingEngine engine;
    int updated = 0;
    engine.evaluateAging(queue, base.addSecs(60), &updated);

    QCOMPARE(updated, 1);
    QCOMPARE(queue.findByFlightId(101)->effectivePriority, 3);
}

// ── 22. Existing Emergency Priority tests continue passing
void TestStarvationSimulation::testExistingEmergencyPriorityIntact()
{
    Models::Process p(1, 101, 4, 30, Models::ProcessState::Ready);
    p.setEmergency(true);
    QCOMPARE(p.effectivePriority, 1);
    QCOMPARE(p.basePriority, 4);

    p.setEmergency(false);
    QCOMPARE(p.effectivePriority, 4);
    QCOMPARE(p.basePriority, 4);
}

// ── 23. Existing Scheduler tests continue passing
void TestStarvationSimulation::testExistingSchedulerIntact()
{
    Scheduler scheduler;
    QDateTime base(QDate(2026, 1, 1), QTime(0, 0, 0));

    Models::Process p1(1, 101, 4, 30, Models::ProcessState::Ready);
    p1.arrivalTime = base;
    p1.readySince = base;

    Models::Process p2(2, 202, 2, 30, Models::ProcessState::Ready);
    p2.arrivalTime = base.addSecs(1);
    p2.readySince = p2.arrivalTime;

    scheduler.readyQueue().enqueue(p1);
    scheduler.readyQueue().enqueue(p2);

    Models::Process candidate;
    QVERIFY(scheduler.selectNextProcess(&candidate));
    QCOMPARE(candidate.flightId, 202); // Priority 2 beats priority 4
}

// ── 24. Non-preemptive scheduling remains intact
void TestStarvationSimulation::testNonPreemptiveSchedulingRemainsIntact()
{
    SimulationConfig config;
    config.mode = SimulationMode::WithAging;
    config.simulationHorizonSeconds = 180;

    StarvationSimulation sim(config);
    SimulationMetrics m = sim.runToHorizon();
    QVERIFY(!m.dispatchEvents.isEmpty());

    // In non-preemptive scheduling, every dispatched process finishes its full burst time uninterrupted
    for (const auto &ev : m.dispatchEvents) {
        QCOMPARE(ev.completionTimeSec - ev.dispatchTimeSec, ev.burstTimeSec);
        QCOMPARE(ev.burstTimeSec, 20);
    }

    // Verify specifically that P3 (dispatched at t=4s) executed continuously until t=24s
    // despite P6 arriving at t=20s (higher priority arrival at t=20s did not preempt running P3)
    QCOMPARE(m.dispatchEvents[0].flightId, 303);
    QCOMPARE(m.dispatchEvents[0].dispatchTimeSec, 4);
    QCOMPARE(m.dispatchEvents[0].completionTimeSec, 24);
}

// ── 25. Invalid simulation configurations are rejected safely
void TestStarvationSimulation::testInvalidSimulationConfigurationRejected()
{
    SimulationConfig badInterval;
    badInterval.agingIntervalSeconds = 0;
    QString err;
    QVERIFY(!badInterval.isValid(&err));

    SimulationConfig badHorizon;
    badHorizon.simulationHorizonSeconds = -10;
    QVERIFY(!badHorizon.isValid(&err));
}

// ── 26. Repeated simulation runs produce identical results
void TestStarvationSimulation::testRepeatedSimulationRunsProduceIdenticalResults()
{
    SimulationMetrics run1 = StarvationSimulation::runStandardSimulation(SimulationMode::WithAging, 30, 180);
    SimulationMetrics run2 = StarvationSimulation::runStandardSimulation(SimulationMode::WithAging, 30, 180);

    QCOMPARE(run1.totalDispatched, run2.totalDispatched);
    QCOMPARE(run1.lowPriorityDispatched, run2.lowPriorityDispatched);
    QCOMPARE(run1.lowPriorityWaitingTime, run2.lowPriorityWaitingTime);
    QCOMPARE(run1.lowPriorityFinalEffectivePriority, run2.lowPriorityFinalEffectivePriority);
    QCOMPARE(run1.dispatchOrder, run2.dispatchOrder);
}

QTEST_MAIN(TestStarvationSimulation)
#include "TestStarvationSimulation.moc"
