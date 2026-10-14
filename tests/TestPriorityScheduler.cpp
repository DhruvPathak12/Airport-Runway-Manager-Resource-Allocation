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

class TestPriorityScheduler : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ── 1. Priority Selection (Unit Tests - Offline) ─────────────────────────
    void testSingleReadyProcessSelected();
    void testPriority1WinsOverLowerPriorities();
    void testPriority3WinsOver4And5();
    void testPriority5SelectedWhenOnlyProcess();
    void testEffectivePriorityDeterminesSelection();
    void testNewlyArrivingHigherPriorityDoesNotInterruptRunningProcess();

    // ── 2. Tie-Breaking (Unit Tests - Offline) ────────────────────────────────
    void testEqualPriorityOrderedByEarlierArrivalTime();
    void testEqualPriorityAndArrivalResolvedBySmallerFlightId();
    void testDeterministicRepeatedRuns();
    void testReadyQueuePreservesFifoInsertionOrder();

    // ── 3. Dispatch and State Consistency (Unit Tests - Offline) ─────────────
    void testExactlyOneProcessDispatchedPerOperation();
    void testSelectedProcessTransitionsFromReadyToRunning();
    void testDispatchedProcessRemovedFromReadyQueue();
    void testOtherReadyProcessesRemainInQueueOrder();
    void testEmptyQueueHandledSafely();
    void testInvalidProcessDataHandledSafely();
    void testIllegalStateTransitionsRejected();
    void testSecondDispatchRejectedWhileProcessIsRunning();
    void testFailedDispatchPreservesQueueAndState();
    void testCompletedProcessNotSelectedAgain();

    // ── 4. Service Layer, RBAC & Persistence (Integration Tests) ─────────────
    void testServiceDispatchRBACEnforcement();
    void testServiceDispatchPersistsToDatabaseIfConnected();
    void testProcessCreationAndDuplicatePreventionRegression();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;
    int                m_testFlightId1 = 0;
    int                m_testFlightId2 = 0;
};

void TestPriorityScheduler::initTestCase()
{
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();

    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'SCHED-%');"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'SCHED-%';"));

            m_service.setTestSession(Session(1, QStringLiteral("atc_tester"), QStringLiteral("ATC Tester"), QStringLiteral("ATC")));

            Flight f1;
            f1.flightNumber = QStringLiteral("SCHED-101");
            f1.airline = QStringLiteral("AeroTest");
            f1.source = QStringLiteral("DEL");
            f1.destination = QStringLiteral("BOM");
            f1.flightType = QStringLiteral("Arrival");
            f1.scheduledTime = QDateTime::currentDateTime().addSecs(1800);
            f1.priority = 3;
            f1.operationalStatus = QStringLiteral("Scheduled");

            if (m_service.createFlight(f1)) {
                QList<Flight> flist = m_service.getAllFlights(QStringLiteral("SCHED-101"));
                if (!flist.isEmpty()) {
                    m_testFlightId1 = flist.first().id;
                }
            }

            Flight f2;
            f2.flightNumber = QStringLiteral("SCHED-102");
            f2.airline = QStringLiteral("AeroTest");
            f2.source = QStringLiteral("BOM");
            f2.destination = QStringLiteral("BLR");
            f2.flightType = QStringLiteral("Departure");
            f2.scheduledTime = QDateTime::currentDateTime().addSecs(3600);
            f2.priority = 1;
            f2.operationalStatus = QStringLiteral("Scheduled");

            if (m_service.createFlight(f2)) {
                QList<Flight> flist = m_service.getAllFlights(QStringLiteral("SCHED-102"));
                if (!flist.isEmpty()) {
                    m_testFlightId2 = flist.first().id;
                }
            }
        }
    }
}

void TestPriorityScheduler::cleanupTestCase()
{
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'SCHED-%');"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'SCHED-%';"));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Priority Selection Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestPriorityScheduler::testSingleReadyProcessSelected()
{
    Scheduler sched;
    Process p(1, 101, 3, 60, ProcessState::Ready);
    p.arrivalTime = QDateTime::currentDateTime();
    QVERIFY(sched.readyQueue().enqueue(p));

    Process selected;
    QString reason;
    bool ok = sched.selectNextProcess(&selected, &reason);

    QVERIFY(ok);
    QCOMPARE(selected.flightId, 101);
    QCOMPARE(selected.effectivePriority, 3);
}

void TestPriorityScheduler::testPriority1WinsOverLowerPriorities()
{
    Scheduler sched;
    QDateTime now = QDateTime::currentDateTime();

    Process p3(1, 103, 3, 60, ProcessState::Ready);
    p3.arrivalTime = now;
    Process p5(2, 105, 5, 60, ProcessState::Ready);
    p5.arrivalTime = now.addSecs(10);
    Process p1(3, 101, 1, 60, ProcessState::Ready);
    p1.arrivalTime = now.addSecs(20);
    Process p2(4, 102, 2, 60, ProcessState::Ready);
    p2.arrivalTime = now.addSecs(30);

    QVERIFY(sched.readyQueue().enqueue(p3));
    QVERIFY(sched.readyQueue().enqueue(p5));
    QVERIFY(sched.readyQueue().enqueue(p1));
    QVERIFY(sched.readyQueue().enqueue(p2));

    Process selected;
    QVERIFY(sched.selectNextProcess(&selected));
    QCOMPARE(selected.flightId, 101);
    QCOMPARE(selected.effectivePriority, 1);
}

void TestPriorityScheduler::testPriority3WinsOver4And5()
{
    Scheduler sched;
    QDateTime now = QDateTime::currentDateTime();

    Process p5(1, 105, 5, 60, ProcessState::Ready);
    p5.arrivalTime = now;
    Process p4(2, 104, 4, 60, ProcessState::Ready);
    p4.arrivalTime = now.addSecs(5);
    Process p3(3, 103, 3, 60, ProcessState::Ready);
    p3.arrivalTime = now.addSecs(10);

    QVERIFY(sched.readyQueue().enqueue(p5));
    QVERIFY(sched.readyQueue().enqueue(p4));
    QVERIFY(sched.readyQueue().enqueue(p3));

    Process selected;
    QVERIFY(sched.selectNextProcess(&selected));
    QCOMPARE(selected.flightId, 103);
    QCOMPARE(selected.effectivePriority, 3);
}

void TestPriorityScheduler::testPriority5SelectedWhenOnlyProcess()
{
    Scheduler sched;
    Process p5(1, 105, 5, 60, ProcessState::Ready);
    p5.arrivalTime = QDateTime::currentDateTime();

    QVERIFY(sched.readyQueue().enqueue(p5));

    Process selected;
    QVERIFY(sched.selectNextProcess(&selected));
    QCOMPARE(selected.flightId, 105);
    QCOMPARE(selected.effectivePriority, 5);
}

void TestPriorityScheduler::testEffectivePriorityDeterminesSelection()
{
    Scheduler sched;
    QDateTime now = QDateTime::currentDateTime();

    // Process A: Base priority 2, but effective priority is 4
    Process pA(1, 101, 2, 60, ProcessState::Ready);
    pA.effectivePriority = 4;
    pA.arrivalTime = now;

    // Process B: Base priority 5, but effective priority is 3 (e.g. adjusted)
    Process pB(2, 102, 5, 60, ProcessState::Ready);
    pB.effectivePriority = 3;
    pB.arrivalTime = now.addSecs(10);

    QVERIFY(sched.readyQueue().enqueue(pA));
    QVERIFY(sched.readyQueue().enqueue(pB));

    Process selected;
    QVERIFY(sched.selectNextProcess(&selected));
    // pB should win because effectivePriority 3 < 4 (even though pA has basePriority 2 < 5)
    QCOMPARE(selected.flightId, 102);
    QCOMPARE(selected.effectivePriority, 3);
}

void TestPriorityScheduler::testNewlyArrivingHigherPriorityDoesNotInterruptRunningProcess()
{
    Scheduler sched;
    QDateTime now = QDateTime::currentDateTime();

    // Flight 103 (Priority 3) starts in queue
    Process p3(1, 103, 3, 60, ProcessState::Ready);
    p3.arrivalTime = now;
    QVERIFY(sched.readyQueue().enqueue(p3));

    // Dispatch flight 103 to RUNNING
    Scheduler::DispatchResult res = sched.dispatchNextProcess();
    QVERIFY(res.success);
    QCOMPARE(res.process.flightId, 103);
    QCOMPARE(res.process.state, ProcessState::Running);
    QVERIFY(sched.hasRunningProcess());
    QCOMPARE(sched.runningProcess().flightId, 103);

    // Now a higher-priority process (Priority 1) arrives in the Ready Queue
    Process p1(2, 101, 1, 60, ProcessState::Ready);
    p1.arrivalTime = now.addSecs(10);
    QVERIFY(sched.readyQueue().enqueue(p1));

    // Under Non-Preemptive scheduling, the running process MUST NOT be interrupted!
    QVERIFY(sched.hasRunningProcess());
    QCOMPARE(sched.runningProcess().flightId, 103);
    QCOMPARE(sched.runningProcess().state, ProcessState::Running);

    // Attempting another dispatch while flight 103 is running must be safely rejected
    Scheduler::DispatchResult secondRes = sched.dispatchNextProcess();
    QVERIFY(!secondRes.success);
    QVERIFY(secondRes.reason.contains(QStringLiteral("is currently RUNNING")));
    QCOMPARE(sched.runningProcess().flightId, 103);
    QCOMPARE(sched.readyQueue().size(), 1);
    QVERIFY(sched.readyQueue().containsFlight(101));
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Tie-Breaking Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestPriorityScheduler::testEqualPriorityOrderedByEarlierArrivalTime()
{
    Scheduler sched;
    QDateTime t1 = QDateTime(QDate(2026, 10, 14), QTime(10, 0, 0));
    QDateTime t2 = QDateTime(QDate(2026, 10, 14), QTime(10, 5, 0));

    // Both priority 2, but pLater was enqueued first
    Process pLater(1, 202, 2, 60, ProcessState::Ready);
    pLater.arrivalTime = t2;

    Process pEarlier(2, 201, 2, 60, ProcessState::Ready);
    pEarlier.arrivalTime = t1;

    QVERIFY(sched.readyQueue().enqueue(pLater));
    QVERIFY(sched.readyQueue().enqueue(pEarlier));

    Process selected;
    QVERIFY(sched.selectNextProcess(&selected));
    // Earlier arrival time (10:00:00) wins over 10:05:00
    QCOMPARE(selected.flightId, 201);
}

void TestPriorityScheduler::testEqualPriorityAndArrivalResolvedBySmallerFlightId()
{
    Scheduler sched;
    QDateTime sameTime = QDateTime(QDate(2026, 10, 14), QTime(12, 0, 0));

    Process pLargeId(1, 999, 3, 60, ProcessState::Ready);
    pLargeId.arrivalTime = sameTime;

    Process pSmallId(2, 111, 3, 60, ProcessState::Ready);
    pSmallId.arrivalTime = sameTime;

    QVERIFY(sched.readyQueue().enqueue(pLargeId));
    QVERIFY(sched.readyQueue().enqueue(pSmallId));

    Process selected;
    QVERIFY(sched.selectNextProcess(&selected));
    // Same priority (3), same arrival time -> smaller flight ID (111) wins
    QCOMPARE(selected.flightId, 111);
}

void TestPriorityScheduler::testDeterministicRepeatedRuns()
{
    Scheduler sched1;
    Scheduler sched2;
    QDateTime now = QDateTime::currentDateTime();

    Process a(1, 50, 2, 60, ProcessState::Ready); a.arrivalTime = now;
    Process b(2, 30, 2, 60, ProcessState::Ready); b.arrivalTime = now;
    Process c(3, 40, 1, 60, ProcessState::Ready); c.arrivalTime = now.addSecs(5);

    sched1.readyQueue().enqueue(a);
    sched1.readyQueue().enqueue(b);
    sched1.readyQueue().enqueue(c);

    sched2.readyQueue().enqueue(a);
    sched2.readyQueue().enqueue(b);
    sched2.readyQueue().enqueue(c);

    Process sel1, sel2;
    QVERIFY(sched1.selectNextProcess(&sel1));
    QVERIFY(sched2.selectNextProcess(&sel2));

    QCOMPARE(sel1.flightId, sel2.flightId);
    QCOMPARE(sel1.flightId, 40); // Priority 1 wins over Priority 2
}

void TestPriorityScheduler::testReadyQueuePreservesFifoInsertionOrder()
{
    Scheduler sched;
    QDateTime now = QDateTime::currentDateTime();

    Process p1(1, 101, 4, 60, ProcessState::Ready); p1.arrivalTime = now;
    Process p2(2, 102, 1, 60, ProcessState::Ready); p2.arrivalTime = now;
    Process p3(3, 103, 3, 60, ProcessState::Ready); p3.arrivalTime = now;

    sched.readyQueue().enqueue(p1);
    sched.readyQueue().enqueue(p2);
    sched.readyQueue().enqueue(p3);

    // Ready queue underlying storage MUST preserve FIFO insertion order
    QList<Process> list = sched.readyQueue().allProcesses();
    QCOMPARE(list.size(), 3);
    QCOMPARE(list.at(0).flightId, 101);
    QCOMPARE(list.at(1).flightId, 102);
    QCOMPARE(list.at(2).flightId, 103);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Dispatch and State Consistency Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestPriorityScheduler::testExactlyOneProcessDispatchedPerOperation()
{
    Scheduler sched;
    QDateTime now = QDateTime::currentDateTime();

    Process p1(1, 101, 2, 60, ProcessState::Ready); p1.arrivalTime = now;
    Process p2(2, 102, 3, 60, ProcessState::Ready); p2.arrivalTime = now.addSecs(1);

    sched.readyQueue().enqueue(p1);
    sched.readyQueue().enqueue(p2);
    QCOMPARE(sched.readyQueue().size(), 2);

    Scheduler::DispatchResult res = sched.dispatchNextProcess();
    QVERIFY(res.success);
    QCOMPARE(res.process.flightId, 101);
    QCOMPARE(res.process.state, ProcessState::Running);

    // Exactly one process dispatched; other remains in queue
    QCOMPARE(sched.readyQueue().size(), 1);
    QVERIFY(sched.hasRunningProcess());
}

void TestPriorityScheduler::testSelectedProcessTransitionsFromReadyToRunning()
{
    Scheduler sched;
    Process p(1, 101, 1, 60, ProcessState::Ready);
    p.arrivalTime = QDateTime::currentDateTime();
    sched.readyQueue().enqueue(p);

    Scheduler::DispatchResult res = sched.dispatchNextProcess();
    QVERIFY(res.success);
    QCOMPARE(res.process.state, ProcessState::Running);
    QCOMPARE(sched.runningProcess().state, ProcessState::Running);
}

void TestPriorityScheduler::testDispatchedProcessRemovedFromReadyQueue()
{
    Scheduler sched;
    Process p(1, 101, 2, 60, ProcessState::Ready);
    p.arrivalTime = QDateTime::currentDateTime();
    sched.readyQueue().enqueue(p);

    QVERIFY(sched.readyQueue().containsFlight(101));
    Scheduler::DispatchResult res = sched.dispatchNextProcess();
    QVERIFY(res.success);

    // Ready queue must NOT contain the dispatched process
    QVERIFY(!sched.readyQueue().containsFlight(101));
    QVERIFY(sched.readyQueue().isEmpty());
}

void TestPriorityScheduler::testOtherReadyProcessesRemainInQueueOrder()
{
    Scheduler sched;
    QDateTime now = QDateTime::currentDateTime();

    // pA: flight 10, prio 3
    Process pA(1, 10, 3, 60, ProcessState::Ready); pA.arrivalTime = now;
    // pB: flight 20, prio 1 (highest)
    Process pB(2, 20, 1, 60, ProcessState::Ready); pB.arrivalTime = now.addSecs(1);
    // pC: flight 30, prio 4
    Process pC(3, 30, 4, 60, ProcessState::Ready); pC.arrivalTime = now.addSecs(2);

    sched.readyQueue().enqueue(pA);
    sched.readyQueue().enqueue(pB);
    sched.readyQueue().enqueue(pC);

    // Dispatch: pB should be dispatched
    Scheduler::DispatchResult res = sched.dispatchNextProcess();
    QVERIFY(res.success);
    QCOMPARE(res.process.flightId, 20);

    // Remaining in queue must be pA followed by pC (relative FIFO order preserved)
    QList<Process> remaining = sched.readyQueue().allProcesses();
    QCOMPARE(remaining.size(), 2);
    QCOMPARE(remaining.at(0).flightId, 10);
    QCOMPARE(remaining.at(1).flightId, 30);
}

void TestPriorityScheduler::testEmptyQueueHandledSafely()
{
    Scheduler sched;
    QVERIFY(sched.readyQueue().isEmpty());

    Process selected;
    QString reason;
    bool selectOk = sched.selectNextProcess(&selected, &reason);
    QVERIFY(!selectOk);
    QVERIFY(reason.contains(QStringLiteral("empty"), Qt::CaseInsensitive));

    Scheduler::DispatchResult res = sched.dispatchNextProcess();
    QVERIFY(!res.success);
    QVERIFY(!sched.hasRunningProcess());
    QVERIFY(res.reason.contains(QStringLiteral("empty"), Qt::CaseInsensitive));
}

void TestPriorityScheduler::testInvalidProcessDataHandledSafely()
{
    Scheduler sched;
    Process valid(1, 101, 3, 60, ProcessState::Ready);
    valid.arrivalTime = QDateTime::currentDateTime();
    sched.readyQueue().enqueue(valid);

    // Attempting to enqueue an invalid process is rejected by the queue
    Process invalid(2, 0, 3, 60, ProcessState::Ready); // Invalid flightId = 0
    QString qErr;
    QVERIFY(!sched.readyQueue().enqueue(invalid, &qErr));
    QVERIFY(qErr.contains(QStringLiteral("flight ID"), Qt::CaseInsensitive));

    // The valid process should still be dispatchable
    Scheduler::DispatchResult res = sched.dispatchNextProcess();
    QVERIFY(res.success);
    QCOMPARE(res.process.flightId, 101);
}

void TestPriorityScheduler::testIllegalStateTransitionsRejected()
{
    // Validate that non-Ready processes cannot be dispatched
    Process running(1, 101, 1, 60, ProcessState::Running);
    QString reason;
    QVERIFY(!running.canTransitionTo(ProcessState::Running, &reason));
    QVERIFY(reason.contains(QStringLiteral("same state"), Qt::CaseInsensitive));

    Process completed(2, 102, 1, 60, ProcessState::Completed);
    QVERIFY(!completed.canTransitionTo(ProcessState::Running, &reason));
    QVERIFY(reason.contains(QStringLiteral("Invalid state transition"), Qt::CaseInsensitive));
}

void TestPriorityScheduler::testSecondDispatchRejectedWhileProcessIsRunning()
{
    Scheduler sched;
    QDateTime now = QDateTime::currentDateTime();

    Process p1(1, 101, 1, 60, ProcessState::Ready); p1.arrivalTime = now;
    Process p2(2, 102, 2, 60, ProcessState::Ready); p2.arrivalTime = now;

    sched.readyQueue().enqueue(p1);
    sched.readyQueue().enqueue(p2);

    Scheduler::DispatchResult res1 = sched.dispatchNextProcess();
    QVERIFY(res1.success);
    QCOMPARE(res1.process.flightId, 101);

    // Second dispatch while first is running is rejected
    Scheduler::DispatchResult res2 = sched.dispatchNextProcess();
    QVERIFY(!res2.success);
    QVERIFY(res2.reason.contains(QStringLiteral("is currently RUNNING")));

    // When the running process completes, clearRunningProcess allows subsequent dispatch
    sched.clearRunningProcess();
    QVERIFY(!sched.hasRunningProcess());

    Scheduler::DispatchResult res3 = sched.dispatchNextProcess();
    QVERIFY(res3.success);
    QCOMPARE(res3.process.flightId, 102);
}

void TestPriorityScheduler::testFailedDispatchPreservesQueueAndState()
{
    Scheduler sched;
    Process p(1, 101, 2, 60, ProcessState::Ready);
    p.arrivalTime = QDateTime::currentDateTime();
    sched.readyQueue().enqueue(p);

    // Set an active running process to trigger a dispatch failure
    Process existingRunning(99, 999, 1, 60, ProcessState::Running);
    sched.setRunningProcess(existingRunning);

    Scheduler::DispatchResult res = sched.dispatchNextProcess();
    QVERIFY(!res.success);

    // The ready queue must still contain flight 101 in its original state
    QCOMPARE(sched.readyQueue().size(), 1);
    const Process *proc = sched.readyQueue().findByFlightId(101);
    QVERIFY(proc != nullptr);
    QCOMPARE(proc->state, ProcessState::Ready);
}

void TestPriorityScheduler::testCompletedProcessNotSelectedAgain()
{
    Scheduler sched;
    Process p(1, 101, 1, 60, ProcessState::Ready);
    p.arrivalTime = QDateTime::currentDateTime();
    sched.readyQueue().enqueue(p);

    // Dispatch and complete the process
    Scheduler::DispatchResult res = sched.dispatchNextProcess();
    QVERIFY(res.success);
    QVERIFY(!sched.readyQueue().containsFlight(101));

    // Clear running process simulating completion
    sched.clearRunningProcess();

    // Now queue is empty; completed process cannot be selected again
    Process sel;
    QString err;
    QVERIFY(!sched.selectNextProcess(&sel, &err));
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Service Layer, RBAC & Persistence Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestPriorityScheduler::testServiceDispatchRBACEnforcement()
{
    ApplicationService service;
    QString err;

    // 1. Unauthenticated invocation is rejected
    QCOMPARE(service.dispatchNextProcess(nullptr, &err), false);
    QVERIFY(err.contains(QStringLiteral("Authentication required")));

    // 2. Airport Operator role is denied access (action:ManageScheduling requires ATC/Admin)
    Session op(3, QStringLiteral("operator"), QStringLiteral("Operator"), QStringLiteral("Airport Operator"));
    service.setTestSession(op);
    err.clear();
    QCOMPARE(service.dispatchNextProcess(nullptr, &err), false);
    QVERIFY(err.contains(QStringLiteral("Access denied")));

    // 3. ATC role is authorized
    Session atc(2, QStringLiteral("atc"), QStringLiteral("ATC User"), QStringLiteral("ATC"));
    service.setTestSession(atc);
    err.clear();
    // With empty queue, authorization succeeds and returns false with empty queue reason
    QCOMPARE(service.dispatchNextProcess(nullptr, &err), false);
    QVERIFY(!err.contains(QStringLiteral("Access denied")));
    QVERIFY(!err.contains(QStringLiteral("Authentication required")));
    QVERIFY(err.contains(QStringLiteral("empty"), Qt::CaseInsensitive));
}

void TestPriorityScheduler::testServiceDispatchPersistsToDatabaseIfConnected()
{
    if (!m_dbConnected || m_testFlightId1 == 0 || m_testFlightId2 == 0) {
        QSKIP("Live MySQL database not available; skipping DB persistence integration test.");
    }

    Session atc(2, QStringLiteral("atc_tester"), QStringLiteral("ATC Tester"), QStringLiteral("ATC"));
    m_service.setTestSession(atc);

    // Clear any leftover state
    m_service.scheduler()->reset();

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (:f1, :f2);"));
        q.bindValue(QStringLiteral(":f1"), m_testFlightId1);
        q.bindValue(QStringLiteral(":f2"), m_testFlightId2);
        q.exec();
    }

    QString err;
    // Create process records for test flights in DB
    Process p1;
    QVERIFY(m_service.createProcessForFlight(m_testFlightId1, 60, &p1, &err));
    Process p2;
    QVERIFY(m_service.createProcessForFlight(m_testFlightId2, 60, &p2, &err));

    // Clear and verify in-memory queue has our 2 processes (createProcessForFlight enqueues them)
    QCOMPARE(m_service.readyQueue()->size(), 2);

    // Dispatch: flightId2 (Priority 1) should be chosen over flightId1 (Priority 3)
    Process dispatched;
    bool ok = m_service.dispatchNextProcess(&dispatched, &err);
    QVERIFY2(ok, qPrintable(err));
    QCOMPARE(dispatched.flightId, m_testFlightId2);
    QCOMPARE(dispatched.state, ProcessState::Running);

    // Check DB record
    Process dbProc;
    QVERIFY(m_service.getProcessByFlightId(m_testFlightId2, dbProc));
    QCOMPARE(dbProc.state, ProcessState::Running);

    // Clean up
    m_service.scheduler()->clearRunningProcess();
}

void TestPriorityScheduler::testProcessCreationAndDuplicatePreventionRegression()
{
    Scheduler sched;
    Process p(1, 555, 2, 60, ProcessState::Ready);
    p.arrivalTime = QDateTime::currentDateTime();

    QVERIFY(sched.readyQueue().enqueue(p));

    // Duplicate enqueue with same flight ID must be rejected
    QString dupErr;
    QVERIFY(!sched.readyQueue().enqueue(p, &dupErr));
    QVERIFY(dupErr.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive));
}

QTEST_MAIN(TestPriorityScheduler)
#include "TestPriorityScheduler.moc"
