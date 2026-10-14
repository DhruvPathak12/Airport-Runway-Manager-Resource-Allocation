#include <QtTest/QtTest>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>

#include "models/Process.h"
#include "models/Flight.h"
#include "models/Session.h"
#include "scheduler/ReadyQueue.h"
#include "scheduler/Scheduler.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"

using namespace Models;
using namespace SchedulerComponents;

class TestSchedulingMetrics : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ── 1. Individual Metric Calculations ────────────────────────────────────
    void testCorrectCompletionTimeCalculation();
    void testCorrectTurnaroundTimeCalculation();
    void testCorrectWaitingTimeCalculation();
    void testZeroWaitingTimeWhenExecutingImmediately();
    void testProcessArrivingWhileAnotherIsRunning();
    void testMultipleProcessesExecutingSequentially();
    void testIdleTimeBeforeProcessArrival();

    // ── 2. Aggregate Metrics (Averages, Extrema, Edge Cases) ─────────────────
    void testCorrectAveragesForMultipleCompletedProcesses();
    void testEmptyCompletedProcessSetReturnsZeroAverages();
    void testSingleCompletedProcessMetrics();
    void testIndependenceFromResultRetrievalOrder();

    // ── 3. Lifecycle Exclusion & State Rules ──────────────────────────────────
    void testExclusionOfReadyProcesses();
    void testExclusionOfRunningProcesses();
    void testExclusionOfBlockedProcesses();
    void testProcessBlockedBeforeCompletionNotTreatedAsCompleted();

    // ── 4. Validation & Safeguards ───────────────────────────────────────────
    void testRejectionOfNegativeOrInconsistentTimingData();
    void testPreventionOfDuplicateMetricRecording();

    // ── 5. Regressions & Service Integration ─────────────────────────────────
    void testPrioritySchedulingAndTieBreakingRemainUnchanged();
    void testQueueConsistencyAndStateTransitionsRemainConsistent();
    void testServiceCompleteRunningProcessRBACAndPersistence();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;
    int                m_testFlightId1 = 0;
};

void TestSchedulingMetrics::initTestCase()
{
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();

    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'METRIC-%');"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'METRIC-%';"));

            m_service.setTestSession(Session(1, QStringLiteral("atc_tester"), QStringLiteral("ATC Tester"), QStringLiteral("ATC")));

            Flight f1;
            f1.flightNumber = QStringLiteral("METRIC-101");
            f1.airline = QStringLiteral("MetricAir");
            f1.source = QStringLiteral("DEL");
            f1.destination = QStringLiteral("BOM");
            f1.flightType = QStringLiteral("Arrival");
            f1.scheduledTime = QDateTime::currentDateTime().addSecs(1800);
            f1.priority = 2;
            f1.operationalStatus = QStringLiteral("Scheduled");

            if (m_service.createFlight(f1)) {
                QList<Flight> flist = m_service.getAllFlights(QStringLiteral("METRIC-101"));
                if (!flist.isEmpty()) {
                    m_testFlightId1 = flist.first().id;
                }
            }
        }
    }
}

void TestSchedulingMetrics::cleanupTestCase()
{
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'METRIC-%');"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'METRIC-%';"));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Individual Metric Calculations
// ─────────────────────────────────────────────────────────────────────────────

void TestSchedulingMetrics::testCorrectCompletionTimeCalculation()
{
    Process p(1, 101, 3, 60, ProcessState::Ready);
    QDateTime at = QDateTime(QDate(2026, 10, 14), QTime(12, 0, 0));
    p.arrivalTime = at;

    // Dispatched at 12:00:10 (waiting 10s), burst 60s
    QDateTime st = at.addSecs(10);
    QDateTime ct = st.addSecs(60);

    QVERIFY(p.calculateMetrics(ct));
    QCOMPARE(p.completionTime, ct);
    QCOMPARE(p.turnaroundTime, 70); // 70 seconds total
    QCOMPARE(p.waitingTime, 10);    // 10 seconds waiting
}

void TestSchedulingMetrics::testCorrectTurnaroundTimeCalculation()
{
    Process p(1, 102, 2, 45, ProcessState::Ready);
    QDateTime at = QDateTime(QDate(2026, 10, 14), QTime(14, 0, 0));
    p.arrivalTime = at;

    // Completed at 14:01:15 (75 seconds later)
    QDateTime ct = at.addSecs(75);

    QVERIFY(p.calculateMetrics(ct));
    // TAT = CT - AT = 75s
    QCOMPARE(p.turnaroundTime, 75);
    // WT = TAT - BT = 75 - 45 = 30s
    QCOMPARE(p.waitingTime, 30);
}

void TestSchedulingMetrics::testCorrectWaitingTimeCalculation()
{
    Process p(1, 103, 1, 90, ProcessState::Ready);
    QDateTime at = QDateTime(QDate(2026, 10, 14), QTime(15, 0, 0));
    p.arrivalTime = at;

    // Started execution at 15:00:25 (WT = 25s)
    QDateTime st = at.addSecs(25);
    QVERIFY(p.calculateMetricsFromStart(st));

    QCOMPARE(p.waitingTime, 25);
    QCOMPARE(p.turnaroundTime, 115); // 25 + 90
    QCOMPARE(p.completionTime, at.addSecs(115));
}

void TestSchedulingMetrics::testZeroWaitingTimeWhenExecutingImmediately()
{
    Process p(1, 104, 1, 50, ProcessState::Ready);
    QDateTime at = QDateTime(QDate(2026, 10, 14), QTime(16, 0, 0));
    p.arrivalTime = at;

    // Dispatched immediately at AT (ST == AT)
    QVERIFY(p.calculateMetricsFromStart(at));

    QCOMPARE(p.waitingTime, 0);
    QCOMPARE(p.turnaroundTime, 50); // TAT == BT when WT == 0
    QCOMPARE(p.completionTime, at.addSecs(50));
}

void TestSchedulingMetrics::testProcessArrivingWhileAnotherIsRunning()
{
    Scheduler sched;
    QDateTime baseTime = QDateTime(QDate(2026, 10, 14), QTime(10, 0, 0));

    // Process 1: Arrives at 10:00:00, burst = 60s
    Process p1(1, 1, 1, 60, ProcessState::Ready);
    p1.arrivalTime = baseTime;
    sched.readyQueue().enqueue(p1);

    // Process 2: Arrives at 10:00:20 (while P1 is running), burst = 40s
    Process p2(2, 2, 2, 40, ProcessState::Ready);
    p2.arrivalTime = baseTime.addSecs(20);
    sched.readyQueue().enqueue(p2);

    // Dispatch P1 at 10:00:00
    Scheduler::DispatchResult res1 = sched.dispatchNextProcess();
    QVERIFY(res1.success);

    // P1 completes at 10:01:00
    QDateTime p1Comp = baseTime.addSecs(60);
    Scheduler::CompletionResult cRes1 = sched.completeRunningProcess(p1Comp);
    QVERIFY(cRes1.success);
    QCOMPARE(cRes1.process.waitingTime, 0);
    QCOMPARE(cRes1.process.turnaroundTime, 60);

    // Dispatch P2 at 10:01:00 (non-preemptive CPU becomes available)
    Scheduler::DispatchResult res2 = sched.dispatchNextProcess();
    QVERIFY(res2.success);
    QCOMPARE(res2.process.flightId, 2);

    // P2 completes at 10:01:40
    QDateTime p2Comp = p1Comp.addSecs(40);
    Scheduler::CompletionResult cRes2 = sched.completeRunningProcess(p2Comp);
    QVERIFY(cRes2.success);

    // P2 arrived at 10:00:20, executed at 10:01:00 -> WT = 40s
    QCOMPARE(cRes2.process.waitingTime, 40);
    // TAT = 10:01:40 - 10:00:20 = 80s (WT 40 + BT 40)
    QCOMPARE(cRes2.process.turnaroundTime, 80);
}

void TestSchedulingMetrics::testMultipleProcessesExecutingSequentially()
{
    Scheduler sched;
    QDateTime t0 = QDateTime(QDate(2026, 10, 14), QTime(9, 0, 0));

    // 3 processes arrive at t0 with priorities 1, 2, 3 and burst 30s each
    Process p1(1, 10, 1, 30, ProcessState::Ready); p1.arrivalTime = t0;
    Process p2(2, 20, 2, 30, ProcessState::Ready); p2.arrivalTime = t0;
    Process p3(3, 30, 3, 30, ProcessState::Ready); p3.arrivalTime = t0;

    sched.readyQueue().enqueue(p1);
    sched.readyQueue().enqueue(p2);
    sched.readyQueue().enqueue(p3);

    // P1: ST=t0, CT=t0+30, WT=0, TAT=30
    sched.dispatchNextProcess();
    sched.completeRunningProcess(t0.addSecs(30));

    // P2: ST=t0+30, CT=t0+60, WT=30, TAT=60
    sched.dispatchNextProcess();
    sched.completeRunningProcess(t0.addSecs(60));

    // P3: ST=t0+60, CT=t0+90, WT=60, TAT=90
    sched.dispatchNextProcess();
    sched.completeRunningProcess(t0.addSecs(90));

    QList<Process> completed = sched.completedProcesses();
    QCOMPARE(completed.size(), 3);

    QCOMPARE(completed.at(0).waitingTime, 0);
    QCOMPARE(completed.at(0).turnaroundTime, 30);

    QCOMPARE(completed.at(1).waitingTime, 30);
    QCOMPARE(completed.at(1).turnaroundTime, 60);

    QCOMPARE(completed.at(2).waitingTime, 60);
    QCOMPARE(completed.at(2).turnaroundTime, 90);
}

void TestSchedulingMetrics::testIdleTimeBeforeProcessArrival()
{
    Scheduler sched;
    QDateTime simStart = QDateTime(QDate(2026, 10, 14), QTime(8, 0, 0));
    // Process arrives after 15 minutes of idle time (08:15:00)
    QDateTime arrival = simStart.addSecs(900);

    Process p(1, 888, 1, 60, ProcessState::Ready);
    p.arrivalTime = arrival;
    sched.readyQueue().enqueue(p);

    sched.dispatchNextProcess();
    // Completes at 08:16:00
    Scheduler::CompletionResult res = sched.completeRunningProcess(arrival.addSecs(60));
    QVERIFY(res.success);

    // WT is measured from process arrival time, NOT simulation start: WT = 0
    QCOMPARE(res.process.waitingTime, 0);
    QCOMPARE(res.process.turnaroundTime, 60);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Aggregate Metrics Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestSchedulingMetrics::testCorrectAveragesForMultipleCompletedProcesses()
{
    Scheduler sched;
    QDateTime t0 = QDateTime(QDate(2026, 10, 14), QTime(12, 0, 0));

    // P1: WT=0, TAT=30
    Process p1(1, 1, 1, 30, ProcessState::Ready); p1.arrivalTime = t0;
    // P2: WT=30, TAT=60
    Process p2(2, 2, 2, 30, ProcessState::Ready); p2.arrivalTime = t0;
    // P3: WT=60, TAT=90
    Process p3(3, 3, 3, 30, ProcessState::Ready); p3.arrivalTime = t0;

    sched.readyQueue().enqueue(p1);
    sched.readyQueue().enqueue(p2);
    sched.readyQueue().enqueue(p3);

    sched.dispatchNextProcess();
    sched.completeRunningProcess(t0.addSecs(30));

    sched.dispatchNextProcess();
    sched.completeRunningProcess(t0.addSecs(60));

    sched.dispatchNextProcess();
    sched.completeRunningProcess(t0.addSecs(90));

    Scheduler::PerformanceMetrics m = sched.calculateAggregateMetrics();

    QCOMPARE(m.completedCount, 3);
    QCOMPARE(m.totalTurnaroundTime, 180); // 30 + 60 + 90
    QCOMPARE(m.totalWaitingTime, 90);     // 0 + 30 + 60
    QCOMPARE(m.averageTurnaroundTime, 60.0); // 180 / 3
    QCOMPARE(m.averageWaitingTime, 30.0);    // 90 / 3
    QCOMPARE(m.minWaitingTime, 0);
    QCOMPARE(m.maxWaitingTime, 60);
    QCOMPARE(m.minTurnaroundTime, 30);
    QCOMPARE(m.maxTurnaroundTime, 90);
}

void TestSchedulingMetrics::testEmptyCompletedProcessSetReturnsZeroAverages()
{
    Scheduler sched;
    Scheduler::PerformanceMetrics m = sched.calculateAggregateMetrics();

    // Must return well-defined 0 values without division by zero
    QCOMPARE(m.completedCount, 0);
    QCOMPARE(m.averageTurnaroundTime, 0.0);
    QCOMPARE(m.averageWaitingTime, 0.0);
    QCOMPARE(m.totalTurnaroundTime, 0);
    QCOMPARE(m.totalWaitingTime, 0);
}

void TestSchedulingMetrics::testSingleCompletedProcessMetrics()
{
    Scheduler sched;
    QDateTime t0 = QDateTime(QDate(2026, 10, 14), QTime(10, 0, 0));

    Process p(1, 101, 1, 45, ProcessState::Ready);
    p.arrivalTime = t0;
    sched.readyQueue().enqueue(p);

    sched.dispatchNextProcess();
    sched.completeRunningProcess(t0.addSecs(45));

    Scheduler::PerformanceMetrics m = sched.calculateAggregateMetrics();
    QCOMPARE(m.completedCount, 1);
    QCOMPARE(m.averageTurnaroundTime, 45.0);
    QCOMPARE(m.averageWaitingTime, 0.0);
    QCOMPARE(m.minWaitingTime, 0);
    QCOMPARE(m.maxWaitingTime, 0);
    QCOMPARE(m.minTurnaroundTime, 45);
    QCOMPARE(m.maxTurnaroundTime, 45);
}

void TestSchedulingMetrics::testIndependenceFromResultRetrievalOrder()
{
    Scheduler sched;
    QDateTime t0 = QDateTime(QDate(2026, 10, 14), QTime(12, 0, 0));

    Process p1(1, 1, 1, 20, ProcessState::Ready); p1.arrivalTime = t0;
    Process p2(2, 2, 2, 40, ProcessState::Ready); p2.arrivalTime = t0;

    sched.readyQueue().enqueue(p1);
    sched.readyQueue().enqueue(p2);

    sched.dispatchNextProcess();
    sched.completeRunningProcess(t0.addSecs(20));

    sched.dispatchNextProcess();
    sched.completeRunningProcess(t0.addSecs(60));

    Scheduler::PerformanceMetrics m1 = sched.calculateAggregateMetrics();
    Scheduler::PerformanceMetrics m2 = sched.calculateAggregateMetrics();

    QCOMPARE(m1.averageTurnaroundTime, m2.averageTurnaroundTime);
    QCOMPARE(m1.averageWaitingTime, m2.averageWaitingTime);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Lifecycle Exclusion & State Rules
// ─────────────────────────────────────────────────────────────────────────────

void TestSchedulingMetrics::testExclusionOfReadyProcesses()
{
    Scheduler sched;
    Process readyProc(1, 10, 2, 60, ProcessState::Ready);
    sched.readyQueue().enqueue(readyProc);

    // Ready processes must NOT be included in completed metrics
    Scheduler::PerformanceMetrics m = sched.calculateAggregateMetrics();
    QCOMPARE(m.completedCount, 0);
    QVERIFY(sched.completedProcesses().isEmpty());
}

void TestSchedulingMetrics::testExclusionOfRunningProcesses()
{
    Scheduler sched;
    Process p(1, 20, 2, 60, ProcessState::Ready);
    sched.readyQueue().enqueue(p);

    sched.dispatchNextProcess();
    QVERIFY(sched.hasRunningProcess());

    // RUNNING processes must NOT be included in completed metrics
    Scheduler::PerformanceMetrics m = sched.calculateAggregateMetrics();
    QCOMPARE(m.completedCount, 0);
    QVERIFY(sched.completedProcesses().isEmpty());
}

void TestSchedulingMetrics::testExclusionOfBlockedProcesses()
{
    Scheduler sched;
    Process p(1, 30, 2, 60, ProcessState::Ready);
    sched.readyQueue().enqueue(p);

    sched.dispatchNextProcess();
    Process blocked;
    QVERIFY(sched.blockRunningProcess(&blocked));
    QCOMPARE(blocked.state, ProcessState::Blocked);

    // BLOCKED processes must NOT be counted as completed
    Scheduler::PerformanceMetrics m = sched.calculateAggregateMetrics();
    QCOMPARE(m.completedCount, 0);
    QVERIFY(sched.completedProcesses().isEmpty());
}

void TestSchedulingMetrics::testProcessBlockedBeforeCompletionNotTreatedAsCompleted()
{
    Scheduler sched;
    Process p(1, 40, 1, 60, ProcessState::Ready);
    sched.readyQueue().enqueue(p);

    sched.dispatchNextProcess();
    sched.blockRunningProcess();

    // Attempting to complete when no process is running fails
    Scheduler::CompletionResult res = sched.completeRunningProcess();
    QVERIFY(!res.success);
    QVERIFY(res.reason.contains(QStringLiteral("No process is currently RUNNING")));
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Validation & Safeguards
// ─────────────────────────────────────────────────────────────────────────────

void TestSchedulingMetrics::testRejectionOfNegativeOrInconsistentTimingData()
{
    Process p(1, 101, 3, 60, ProcessState::Ready);
    QDateTime at = QDateTime(QDate(2026, 10, 14), QTime(12, 0, 0));
    p.arrivalTime = at;

    QString err;
    // 1. Completion time preceding arrival time
    QCOMPARE(p.calculateMetrics(at.addSecs(-10), &err), false);
    QVERIFY(err.contains(QStringLiteral("cannot precede arrival time")));

    // 2. Turnaround time shorter than service time (impossible execution)
    err.clear();
    QCOMPARE(p.calculateMetrics(at.addSecs(30), &err), false); // service is 60s
    QVERIFY(err.contains(QStringLiteral("cannot be less than service burst duration")));

    // 3. Invalid date
    err.clear();
    QCOMPARE(p.calculateMetrics(QDateTime(), &err), false);
    QVERIFY(err.contains(QStringLiteral("valid date and time")));
}

void TestSchedulingMetrics::testPreventionOfDuplicateMetricRecording()
{
    Scheduler sched;
    Process p(1, 101, 1, 60, ProcessState::Ready);
    sched.readyQueue().enqueue(p);

    sched.dispatchNextProcess();
    Scheduler::CompletionResult res1 = sched.completeRunningProcess();
    QVERIFY(res1.success);

    // Attempting to complete the same process object again must be rejected
    Scheduler::CompletionResult res2 = sched.completeProcess(res1.process);
    QVERIFY(!res2.success);
    QVERIFY(res2.reason.contains(QStringLiteral("has already been completed")));
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. Regressions & Service Integration
// ─────────────────────────────────────────────────────────────────────────────

void TestSchedulingMetrics::testPrioritySchedulingAndTieBreakingRemainUnchanged()
{
    Scheduler sched;
    QDateTime t0 = QDateTime(QDate(2026, 10, 14), QTime(10, 0, 0));

    // Process A: Priority 3, Arrival t0
    Process pA(1, 10, 3, 30, ProcessState::Ready); pA.arrivalTime = t0;
    // Process B: Priority 1 (highest), Arrival t0+10
    Process pB(2, 20, 1, 30, ProcessState::Ready); pB.arrivalTime = t0.addSecs(10);
    // Process C: Priority 3, Arrival t0 (ties with pA -> smaller flightId wins)
    Process pC(3, 5, 3, 30, ProcessState::Ready); pC.arrivalTime = t0;

    sched.readyQueue().enqueue(pA);
    sched.readyQueue().enqueue(pB);
    sched.readyQueue().enqueue(pC);

    // Priority 1 must win first
    Process sel;
    QVERIFY(sched.selectNextProcess(&sel));
    QCOMPARE(sel.flightId, 20);

    // Dispatch pB and complete it
    sched.dispatchNextProcess();
    sched.completeRunningProcess(t0.addSecs(40));

    // Next candidate among pA and pC: both prio 3, same AT -> flightId 5 wins over 10
    QVERIFY(sched.selectNextProcess(&sel));
    QCOMPARE(sel.flightId, 5);
}

void TestSchedulingMetrics::testQueueConsistencyAndStateTransitionsRemainConsistent()
{
    Scheduler sched;
    Process p(1, 101, 2, 60, ProcessState::Ready);
    sched.readyQueue().enqueue(p);

    QCOMPARE(sched.readyQueue().size(), 1);
    sched.dispatchNextProcess();
    QCOMPARE(sched.readyQueue().size(), 0);
    QVERIFY(sched.hasRunningProcess());

    sched.completeRunningProcess();
    QVERIFY(!sched.hasRunningProcess());
    QCOMPARE(sched.completedProcesses().size(), 1);
}

void TestSchedulingMetrics::testServiceCompleteRunningProcessRBACAndPersistence()
{
    ApplicationService service;
    QString err;

    // 1. Unauthenticated invocation rejected
    QCOMPARE(service.completeRunningProcess(QDateTime(), nullptr, &err), false);
    QVERIFY(err.contains(QStringLiteral("Authentication required")));

    // 2. Operator role denied
    Session op(3, QStringLiteral("op"), QStringLiteral("Operator"), QStringLiteral("Airport Operator"));
    service.setTestSession(op);
    err.clear();
    QCOMPARE(service.completeRunningProcess(QDateTime(), nullptr, &err), false);
    QVERIFY(err.contains(QStringLiteral("Access denied")));

    // 3. Live MySQL test if available
    if (m_dbConnected && m_testFlightId1 > 0) {
        Session atc(2, QStringLiteral("atc_tester"), QStringLiteral("ATC Tester"), QStringLiteral("ATC"));
        m_service.setTestSession(atc);
        m_service.scheduler()->reset();

        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id = :fid;"));
            q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
            q.exec();
        }

        Process p;
        QVERIFY(m_service.createProcessForFlight(m_testFlightId1, 60, &p, &err));

        // Dispatch to RUNNING
        Process dispatched;
        QVERIFY(m_service.dispatchNextProcess(&dispatched, &err));
        QCOMPARE(dispatched.state, ProcessState::Running);

        // Complete the process
        Process completed;
        QDateTime compTime = dispatched.arrivalTime.addSecs(75);
        QVERIFY2(m_service.completeRunningProcess(compTime, &completed, &err), qPrintable(err));
        QCOMPARE(completed.state, ProcessState::Completed);
        QCOMPARE(completed.turnaroundTime, 75);
        QCOMPARE(completed.waitingTime, 15);

        // Verify DB persistence of state and metrics
        Process dbProc;
        QVERIFY(m_service.getProcessByFlightId(m_testFlightId1, dbProc));
        QCOMPARE(dbProc.state, ProcessState::Completed);
        QCOMPARE(dbProc.turnaroundTime, 75);
        QCOMPARE(dbProc.waitingTime, 15);
        QVERIFY(dbProc.completionTime.isValid());

        // Verify aggregate metrics from service
        Scheduler::PerformanceMetrics agg = m_service.getAggregateSchedulingMetrics();
        QCOMPARE(agg.completedCount, 1);
        QCOMPARE(agg.averageTurnaroundTime, 75.0);
        QCOMPARE(agg.averageWaitingTime, 15.0);
    }
}

QTEST_MAIN(TestSchedulingMetrics)
#include "TestSchedulingMetrics.moc"
