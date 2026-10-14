#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>

#include "models/Process.h"
#include "models/Flight.h"
#include "models/Session.h"
#include "scheduler/ReadyQueue.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"

using namespace Models;
using namespace SchedulerComponents;

class TestReadyQueueAndProcessStates : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ── 1. Process State Lifecycle & Transitions (Unit Tests - Offline) ──────
    void testValidStateInitialization();
    void testSupportedValidTransitions();
    void testRejectionOfInvalidTransitions();
    void testOriginalStateUnchangedAfterRejectedTransition();
    void testCppStateToDbStringMapping();
    void testDbStringToCppStateMappingAndInvalidHandling();

    // ── 2. ReadyQueue Component Tests (Unit Tests - Offline) ──────────────────
    void testAddValidReadyProcess();
    void testFifoInsertionOrderPreserved();
    void testFindingProcessByIdentifiers();
    void testRemovingProcessSuccessfully();
    void testRemovingNonexistentProcess();
    void testRejectingDuplicateInsertion();
    void testRejectingInvalidProcessData();
    void testRejectingNonReadyStateInsertion();
    void testQueueSizeAndEmptyBehavior();
    void testQueueMembershipAfterRemoval();
    void testProcessTransitionOutOfReadyRemovesFromQueue();
    void testNoQueueSortingByPriorityOrServiceTime();

    // ── 3. ApplicationService Integration & RBAC Tests (Live MySQL if available)
    void testServiceUpdateProcessStateRBAC();
    void testServiceEnqueueAndDequeueIntegration();
    void testServiceSyncReadyQueueFromDatabase();
    void testServiceFlightDeletionRemovesFromReadyQueue();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;
    int                m_testFlightId1 = 0;
    int                m_testFlightId2 = 0;
};

void TestReadyQueueAndProcessStates::initTestCase()
{
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();

    if (!m_dbConnected) {
        qWarning() << "[TestReadyQueueAndProcessStates] Live DB offline; running standalone unit tests only.";
        return;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'RQ-%');"));
        q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'RQ-%';"));

        m_service.setTestSession(Session(1, QStringLiteral("atc_tester"), QStringLiteral("ATC Tester"), QStringLiteral("ATC")));

        Flight f1;
        f1.flightNumber = QStringLiteral("RQ-101");
        f1.airline = QStringLiteral("SkyWay");
        f1.source = QStringLiteral("BOM");
        f1.destination = QStringLiteral("DEL");
        f1.flightType = QStringLiteral("Arrival");
        f1.scheduledTime = QDateTime::currentDateTime().addSecs(1200);
        f1.priority = 2;
        f1.operationalStatus = QStringLiteral("Scheduled");

        if (m_service.createFlight(f1)) {
            QList<Flight> flist = m_service.getAllFlights(QStringLiteral("RQ-101"));
            if (!flist.isEmpty()) {
                m_testFlightId1 = flist.first().id;
            }
        }

        Flight f2;
        f2.flightNumber = QStringLiteral("RQ-102");
        f2.airline = QStringLiteral("SkyWay");
        f2.source = QStringLiteral("DEL");
        f2.destination = QStringLiteral("BLR");
        f2.flightType = QStringLiteral("Departure");
        f2.scheduledTime = QDateTime::currentDateTime().addSecs(2400);
        f2.priority = 4;
        f2.operationalStatus = QStringLiteral("Scheduled");

        if (m_service.createFlight(f2)) {
            QList<Flight> flist = m_service.getAllFlights(QStringLiteral("RQ-102"));
            if (!flist.isEmpty()) {
                m_testFlightId2 = flist.first().id;
            }
        }
    }
}

void TestReadyQueueAndProcessStates::cleanupTestCase()
{
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'RQ-%');"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'RQ-%';"));
        }
    }
}

// ── 1. Process State Lifecycle & Transitions ─────────────────────────────────

void TestReadyQueueAndProcessStates::testValidStateInitialization()
{
    // Default Process constructor initializes to Ready (matching Day 9 baseline and DB schema)
    Process p;
    QCOMPARE(p.processState(), ProcessState::Ready);
    QCOMPARE(p.processStateString(), QStringLiteral("Ready"));

    // Construction with an explicit New state (for pre-queue in-memory flight mapping)
    Process pNew(0, 100, 2, 30, ProcessState::New);
    QCOMPARE(pNew.processState(), ProcessState::New);
    QCOMPARE(pNew.processStateString(), QStringLiteral("New"));

    // Construction with an explicit Ready state
    Process pReady(10, 101, 2, 30, ProcessState::Ready);
    QCOMPARE(pReady.processState(), ProcessState::Ready);
    QCOMPARE(pReady.processStateString(), QStringLiteral("Ready"));
}

void TestReadyQueueAndProcessStates::testSupportedValidTransitions()
{
    // Lifecycle path 1: New -> Ready -> Running -> Completed
    Process p1(1, 101, 2, 30, ProcessState::New);
    QString err;

    QVERIFY2(p1.canTransitionTo(ProcessState::Ready, &err), qPrintable(err));
    QVERIFY(p1.transitionTo(ProcessState::Ready, &err));
    QCOMPARE(p1.processState(), ProcessState::Ready);

    QVERIFY2(p1.canTransitionTo(ProcessState::Running, &err), qPrintable(err));
    QVERIFY(p1.transitionTo(ProcessState::Running, &err));
    QCOMPARE(p1.processState(), ProcessState::Running);

    QVERIFY2(p1.canTransitionTo(ProcessState::Completed, &err), qPrintable(err));
    QVERIFY(p1.transitionTo(ProcessState::Completed, &err));
    QCOMPARE(p1.processState(), ProcessState::Completed);

    // Lifecycle path 2: Running -> Blocked -> Ready
    Process p2(2, 102, 3, 25, ProcessState::Running);
    QVERIFY2(p2.canTransitionTo(ProcessState::Blocked, &err), qPrintable(err));
    QVERIFY(p2.transitionTo(ProcessState::Blocked, &err));
    QCOMPARE(p2.processState(), ProcessState::Blocked);

    QVERIFY2(p2.canTransitionTo(ProcessState::Ready, &err), qPrintable(err));
    QVERIFY(p2.transitionTo(ProcessState::Ready, &err));
    QCOMPARE(p2.processState(), ProcessState::Ready);

    // Transitioning to the same state must be rejected
    QVERIFY(!p2.canTransitionTo(ProcessState::Ready, &err));
    QVERIFY(!p2.transitionTo(ProcessState::Ready, &err));
    QVERIFY(err.contains(QStringLiteral("same state"), Qt::CaseInsensitive));
    QCOMPARE(p2.processState(), ProcessState::Ready);
}

void TestReadyQueueAndProcessStates::testRejectionOfInvalidTransitions()
{
    QString err;

    // 1. New cannot jump straight to Running, Blocked, or Completed
    Process pNew(1, 101, 1, 30, ProcessState::New);
    QVERIFY(!pNew.canTransitionTo(ProcessState::Running, &err));
    QVERIFY(!pNew.transitionTo(ProcessState::Running, &err));
    QVERIFY(!pNew.canTransitionTo(ProcessState::Blocked, &err));
    QVERIFY(!pNew.transitionTo(ProcessState::Blocked, &err));
    QVERIFY(!pNew.canTransitionTo(ProcessState::Completed, &err));
    QVERIFY(!pNew.transitionTo(ProcessState::Completed, &err));

    // 2. Ready cannot jump to Completed, Blocked, or back to New
    Process pReady(2, 102, 1, 30, ProcessState::Ready);
    QVERIFY(!pReady.canTransitionTo(ProcessState::Completed, &err));
    QVERIFY(!pReady.transitionTo(ProcessState::Completed, &err));
    QVERIFY(!pReady.canTransitionTo(ProcessState::Blocked, &err));
    QVERIFY(!pReady.transitionTo(ProcessState::Blocked, &err));
    QVERIFY(!pReady.canTransitionTo(ProcessState::New, &err));
    QVERIFY(!pReady.transitionTo(ProcessState::New, &err));

    // 3. Blocked cannot jump directly to Running, Completed, or New
    Process pBlocked(3, 103, 1, 30, ProcessState::Blocked);
    QVERIFY(!pBlocked.canTransitionTo(ProcessState::Running, &err));
    QVERIFY(!pBlocked.transitionTo(ProcessState::Running, &err));
    QVERIFY(!pBlocked.canTransitionTo(ProcessState::Completed, &err));
    QVERIFY(!pBlocked.transitionTo(ProcessState::Completed, &err));
    QVERIFY(!pBlocked.canTransitionTo(ProcessState::New, &err));
    QVERIFY(!pBlocked.transitionTo(ProcessState::New, &err));

    // 4. Completed is terminal — cannot transition to New, Ready, Running, or Blocked
    Process pCompleted(4, 104, 1, 30, ProcessState::Completed);
    QVERIFY(!pCompleted.canTransitionTo(ProcessState::New, &err));
    QVERIFY(!pCompleted.transitionTo(ProcessState::New, &err));
    QVERIFY(!pCompleted.canTransitionTo(ProcessState::Ready, &err));
    QVERIFY(!pCompleted.transitionTo(ProcessState::Ready, &err));
    QVERIFY(!pCompleted.canTransitionTo(ProcessState::Running, &err));
    QVERIFY(!pCompleted.transitionTo(ProcessState::Running, &err));
    QVERIFY(!pCompleted.canTransitionTo(ProcessState::Blocked, &err));
    QVERIFY(!pCompleted.transitionTo(ProcessState::Blocked, &err));
}

void TestReadyQueueAndProcessStates::testOriginalStateUnchangedAfterRejectedTransition()
{
    Process p(1, 101, 2, 45, ProcessState::Ready);
    QString err;

    // Attempt invalid transition to Completed
    bool ok = p.transitionTo(ProcessState::Completed, &err);
    QVERIFY(!ok);
    QVERIFY(!err.isEmpty());

    // State MUST remain unchanged as Ready
    QCOMPARE(p.processState(), ProcessState::Ready);
    QCOMPARE(p.processStateString(), QStringLiteral("Ready"));

    // Attempt invalid transition to New
    ok = p.transitionTo(ProcessState::New, &err);
    QVERIFY(!ok);
    QCOMPARE(p.processState(), ProcessState::Ready);
}

void TestReadyQueueAndProcessStates::testCppStateToDbStringMapping()
{
    // Ready -> "Ready"
    QCOMPARE(Process::stateToDbString(ProcessState::Ready), QStringLiteral("Ready"));
    // Running -> "Running"
    QCOMPARE(Process::stateToDbString(ProcessState::Running), QStringLiteral("Running"));
    // Blocked -> "Blocked"
    QCOMPARE(Process::stateToDbString(ProcessState::Blocked), QStringLiteral("Blocked"));
    // Completed -> "Completed"
    QCOMPARE(Process::stateToDbString(ProcessState::Completed), QStringLiteral("Completed"));
    // New maps to "Ready" for DB enum schema compatibility
    QCOMPARE(Process::stateToDbString(ProcessState::New), QStringLiteral("Ready"));
}

void TestReadyQueueAndProcessStates::testDbStringToCppStateMappingAndInvalidHandling()
{
    // Valid DB string parses (case-insensitive)
    QCOMPARE(Process::stateFromDbString(QStringLiteral("Ready")), ProcessState::Ready);
    QCOMPARE(Process::stateFromDbString(QStringLiteral("ready")), ProcessState::Ready);
    QCOMPARE(Process::stateFromDbString(QStringLiteral("Running")), ProcessState::Running);
    QCOMPARE(Process::stateFromDbString(QStringLiteral("RUNNING")), ProcessState::Running);
    QCOMPARE(Process::stateFromDbString(QStringLiteral("Blocked")), ProcessState::Blocked);
    QCOMPARE(Process::stateFromDbString(QStringLiteral("blocked")), ProcessState::Blocked);
    QCOMPARE(Process::stateFromDbString(QStringLiteral("Completed")), ProcessState::Completed);
    QCOMPARE(Process::stateFromDbString(QStringLiteral("completed")), ProcessState::Completed);
    QCOMPARE(Process::stateFromDbString(QStringLiteral("New")), ProcessState::New);

    // Unsupported or corrupted strings must fall back safely to Ready without throwing
    QCOMPARE(Process::stateFromDbString(QStringLiteral("")), ProcessState::Ready);
    QCOMPARE(Process::stateFromDbString(QStringLiteral("Unknown")), ProcessState::Ready);
    QCOMPARE(Process::stateFromDbString(QStringLiteral("KILLED")), ProcessState::Ready);
    QCOMPARE(Process::stateFromDbString(QStringLiteral("Zombie")), ProcessState::Ready);
}

// ── 2. ReadyQueue Component Tests ─────────────────────────────────────────────

void TestReadyQueueAndProcessStates::testAddValidReadyProcess()
{
    ReadyQueue queue;
    QVERIFY(queue.isEmpty());
    QCOMPARE(queue.size(), 0);

    Process p(1, 101, 2, 30, ProcessState::Ready);
    QString err;
    QVERIFY2(queue.enqueue(p, &err), qPrintable(err));

    QCOMPARE(queue.size(), 1);
    QVERIFY(!queue.isEmpty());
    QVERIFY(queue.containsFlight(101));
    QVERIFY(queue.containsQueueId(1));
}

void TestReadyQueueAndProcessStates::testFifoInsertionOrderPreserved()
{
    ReadyQueue queue;

    // Insert 4 processes in a deliberate order with varying priorities & service times
    // P1: priority 4, serviceTime 50
    // P2: priority 1, serviceTime 10 (higher priority, smaller burst)
    // P3: priority 5, serviceTime 20
    // P4: priority 2, serviceTime 15
    Process p1(1, 101, 4, 50, ProcessState::Ready);
    Process p2(2, 102, 1, 10, ProcessState::Ready);
    Process p3(3, 103, 5, 20, ProcessState::Ready);
    Process p4(4, 104, 2, 15, ProcessState::Ready);

    QVERIFY(queue.enqueue(p1));
    QVERIFY(queue.enqueue(p2));
    QVERIFY(queue.enqueue(p3));
    QVERIFY(queue.enqueue(p4));

    QCOMPARE(queue.size(), 4);

    // Verify queue snapshot preserves exact FIFO insertion order
    QList<Process> snapshot = queue.toProcessList();
    QCOMPARE(snapshot.size(), 4);
    QCOMPARE(snapshot.at(0).flightId, 101);
    QCOMPARE(snapshot.at(1).flightId, 102);
    QCOMPARE(snapshot.at(2).flightId, 103);
    QCOMPARE(snapshot.at(3).flightId, 104);

    // Dequeue in FIFO order
    Process out1, out2, out3, out4;
    QVERIFY(queue.dequeue(&out1));
    QCOMPARE(out1.flightId, 101);

    QVERIFY(queue.dequeue(&out2));
    QCOMPARE(out2.flightId, 102);

    QVERIFY(queue.dequeue(&out3));
    QCOMPARE(out3.flightId, 103);

    QVERIFY(queue.dequeue(&out4));
    QCOMPARE(out4.flightId, 104);

    QVERIFY(queue.isEmpty());
}

void TestReadyQueueAndProcessStates::testFindingProcessByIdentifiers()
{
    ReadyQueue queue;
    Process p(15, 250, 3, 40, ProcessState::Ready);
    queue.enqueue(p);

    // Lookup by flightId
    Process foundFlight;
    QVERIFY(queue.findByFlightId(250, &foundFlight));
    QCOMPARE(foundFlight.queueId, 15);
    QCOMPARE(foundFlight.flightId, 250);

    // Lookup by queueId
    Process foundQueue;
    QVERIFY(queue.findByQueueId(15, &foundQueue));
    QCOMPARE(foundQueue.flightId, 250);

    // Lookup nonexistent identifiers
    Process dummy;
    QVERIFY(!queue.findByFlightId(999, &dummy));
    QVERIFY(!queue.findByQueueId(999, &dummy));
}

void TestReadyQueueAndProcessStates::testRemovingProcessSuccessfully()
{
    ReadyQueue queue;
    Process p1(1, 101, 2, 30, ProcessState::Ready);
    Process p2(2, 102, 3, 25, ProcessState::Ready);
    queue.enqueue(p1);
    queue.enqueue(p2);

    // Remove by flightId
    Process removed;
    QVERIFY(queue.removeByFlightId(101, &removed));
    QCOMPARE(removed.flightId, 101);
    QCOMPARE(queue.size(), 1);
    QVERIFY(!queue.containsFlight(101));
    QVERIFY(queue.containsFlight(102));

    // Remove by queueId
    QVERIFY(queue.removeByQueueId(2, &removed));
    QCOMPARE(removed.flightId, 102);
    QCOMPARE(queue.size(), 0);
    QVERIFY(queue.isEmpty());
}

void TestReadyQueueAndProcessStates::testRemovingNonexistentProcess()
{
    ReadyQueue queue;
    Process p(1, 101, 2, 30, ProcessState::Ready);
    queue.enqueue(p);

    QString err;
    QVERIFY(!queue.removeByFlightId(999, nullptr, &err));
    QVERIFY(!err.isEmpty());

    QVERIFY(!queue.removeByQueueId(999, nullptr, &err));
    QVERIFY(!err.isEmpty());

    // Queue size remains intact
    QCOMPARE(queue.size(), 1);
}

void TestReadyQueueAndProcessStates::testRejectingDuplicateInsertion()
{
    ReadyQueue queue;
    Process p1(1, 101, 2, 30, ProcessState::Ready);
    QVERIFY(queue.enqueue(p1));

    // Duplicate flightId (different queueId)
    Process pDupFlight(2, 101, 3, 20, ProcessState::Ready);
    QString err;
    QVERIFY(!queue.enqueue(pDupFlight, &err));
    QVERIFY(err.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive));
    QCOMPARE(queue.size(), 1);

    // Duplicate queueId (different flightId)
    Process pDupQueue(1, 102, 3, 20, ProcessState::Ready);
    QVERIFY(!queue.enqueue(pDupQueue, &err));
    QVERIFY(err.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive));
    QCOMPARE(queue.size(), 1);
}

void TestReadyQueueAndProcessStates::testRejectingInvalidProcessData()
{
    ReadyQueue queue;
    QString err;

    // Invalid priority (0 is out of range 1-5)
    Process pBadPriority(1, 101, 0, 30, ProcessState::Ready);
    QVERIFY(!queue.enqueue(pBadPriority, &err));
    QVERIFY(!err.isEmpty());

    // Invalid service time (0 is out of range 1-3600)
    Process pBadServiceTime(2, 102, 3, 0, ProcessState::Ready);
    QVERIFY(!queue.enqueue(pBadServiceTime, &err));
    QVERIFY(!err.isEmpty());

    // Invalid flight ID (<= 0)
    Process pBadFlightId(3, -5, 3, 30, ProcessState::Ready);
    QVERIFY(!queue.enqueue(pBadFlightId, &err));
    QVERIFY(!err.isEmpty());

    QVERIFY(queue.isEmpty());
}

void TestReadyQueueAndProcessStates::testRejectingNonReadyStateInsertion()
{
    ReadyQueue queue;
    QString err;

    // New state cannot be enqueued directly into ReadyQueue
    Process pNew(1, 101, 2, 30, ProcessState::New);
    QVERIFY(!queue.enqueue(pNew, &err));
    QVERIFY(err.contains(QStringLiteral("state"), Qt::CaseInsensitive));

    // Running state cannot be enqueued into ReadyQueue
    Process pRunning(2, 102, 2, 30, ProcessState::Running);
    QVERIFY(!queue.enqueue(pRunning, &err));

    // Blocked state cannot be enqueued into ReadyQueue
    Process pBlocked(3, 103, 2, 30, ProcessState::Blocked);
    QVERIFY(!queue.enqueue(pBlocked, &err));

    // Completed state cannot be enqueued into ReadyQueue
    Process pCompleted(4, 104, 2, 30, ProcessState::Completed);
    QVERIFY(!queue.enqueue(pCompleted, &err));

    QVERIFY(queue.isEmpty());
}

void TestReadyQueueAndProcessStates::testQueueSizeAndEmptyBehavior()
{
    ReadyQueue queue;
    QVERIFY(queue.isEmpty());
    QCOMPARE(queue.size(), 0);

    // Dequeue on empty returns false
    Process p;
    QString err;
    QVERIFY(!queue.dequeue(&p, &err));
    QVERIFY(!err.isEmpty());

    // Clear on empty
    queue.clear();
    QCOMPARE(queue.size(), 0);

    // Add and clear
    queue.enqueue(Process(1, 101, 2, 30, ProcessState::Ready));
    queue.enqueue(Process(2, 102, 2, 30, ProcessState::Ready));
    QCOMPARE(queue.size(), 2);
    queue.clear();
    QCOMPARE(queue.size(), 0);
    QVERIFY(queue.isEmpty());
}

void TestReadyQueueAndProcessStates::testQueueMembershipAfterRemoval()
{
    ReadyQueue queue;
    Process p1(1, 101, 1, 30, ProcessState::Ready);
    Process p2(2, 102, 2, 30, ProcessState::Ready);
    Process p3(3, 103, 3, 30, ProcessState::Ready);

    queue.enqueue(p1);
    queue.enqueue(p2);
    queue.enqueue(p3);

    // Remove middle element (p2)
    QVERIFY(queue.removeByFlightId(102));

    QCOMPARE(queue.size(), 2);
    QVERIFY(queue.containsFlight(101));
    QVERIFY(!queue.containsFlight(102));
    QVERIFY(queue.containsFlight(103));

    // Remaining items preserve FIFO order
    Process out1, out2;
    QVERIFY(queue.dequeue(&out1));
    QCOMPARE(out1.flightId, 101);

    QVERIFY(queue.dequeue(&out2));
    QCOMPARE(out2.flightId, 103);

    QVERIFY(queue.isEmpty());
}

void TestReadyQueueAndProcessStates::testProcessTransitionOutOfReadyRemovesFromQueue()
{
    ReadyQueue queue;
    Process p1(1, 101, 2, 30, ProcessState::Ready);
    Process p2(2, 102, 3, 20, ProcessState::Ready);

    queue.enqueue(p1);
    queue.enqueue(p2);
    QCOMPARE(queue.size(), 2);

    // Transition P1 to Running using transitionProcessState
    Process updatedP1;
    QString err;
    bool ok = queue.transitionProcessState(101, ProcessState::Running, &updatedP1, &err);
    QVERIFY2(ok, qPrintable(err));
    QCOMPARE(updatedP1.processState(), ProcessState::Running);

    // P1 moved out of Ready -> MUST no longer be in ready queue!
    QCOMPARE(queue.size(), 1);
    QVERIFY(!queue.containsFlight(101));
    QVERIFY(queue.containsFlight(102));

    // Trying to transition P1 from Ready again fails because it is no longer in the queue
    ok = queue.transitionProcessState(101, ProcessState::Completed, nullptr, &err);
    QVERIFY(!ok);

    // Transitioning P2 with an invalid target state (e.g. Completed) fails and leaves P2 in queue
    ok = queue.transitionProcessState(102, ProcessState::Completed, nullptr, &err);
    QVERIFY(!ok);
    QCOMPARE(queue.size(), 1);
    QVERIFY(queue.containsFlight(102));
}

void TestReadyQueueAndProcessStates::testNoQueueSortingByPriorityOrServiceTime()
{
    ReadyQueue queue;

    // Enqueue processes with reversed priorities: P5 (lowest), P4, P3, P2, P1 (highest)
    // Day 10 explicitly mandates FIFO order and NO sorting.
    queue.enqueue(Process(1, 201, 5, 100, ProcessState::Ready));
    queue.enqueue(Process(2, 202, 4, 80, ProcessState::Ready));
    queue.enqueue(Process(3, 203, 3, 60, ProcessState::Ready));
    queue.enqueue(Process(4, 204, 2, 40, ProcessState::Ready));
    queue.enqueue(Process(5, 205, 1, 20, ProcessState::Ready));

    QList<Process> list = queue.toProcessList();
    QCOMPARE(list.size(), 5);

    // Verify first out is P5 (flight 201), NOT P1 (flight 205)
    Process firstOut;
    QVERIFY(queue.dequeue(&firstOut));
    QCOMPARE(firstOut.flightId, 201);
    QCOMPARE(firstOut.priority(), 5);
}

// ── 3. ApplicationService Integration & RBAC Tests ────────────────────────────

void TestReadyQueueAndProcessStates::testServiceUpdateProcessStateRBAC()
{
    // Unauthenticated user denied
    m_service.logout();
    QString err;
    bool ok = m_service.updateProcessState(1, ProcessState::Running, &err);
    QVERIFY(!ok);
    QVERIFY(err.contains(QStringLiteral("authenticated"), Qt::CaseInsensitive));

    // Airport Operator denied (ATC and Admin only)
    m_service.setTestSession(Session(2, QStringLiteral("operator_user"), QStringLiteral("Operator"), QStringLiteral("Airport Operator")));
    ok = m_service.updateProcessState(1, ProcessState::Running, &err);
    QVERIFY(!ok);
    QVERIFY(err.contains(QStringLiteral("denied"), Qt::CaseInsensitive) || err.contains(QStringLiteral("authorized"), Qt::CaseInsensitive));

    // ATC permitted
    m_service.setTestSession(Session(1, QStringLiteral("atc_user"), QStringLiteral("ATC User"), QStringLiteral("ATC")));
    // Note: If ID 99999 doesn't exist, it should pass RBAC and fail on process lookup, NOT on permissions
    ok = m_service.updateProcessState(99999, ProcessState::Running, &err);
    QVERIFY(!ok);
    QVERIFY(!err.contains(QStringLiteral("authorized"), Qt::CaseInsensitive));
    QVERIFY(!err.contains(QStringLiteral("denied"), Qt::CaseInsensitive));
}

void TestReadyQueueAndProcessStates::testServiceEnqueueAndDequeueIntegration()
{
    // Verify in-memory ready queue accessed through ApplicationService
    m_service.setTestSession(Session(1, QStringLiteral("atc_user"), QStringLiteral("ATC User"), QStringLiteral("ATC")));

    ReadyQueue* rq = m_service.readyQueue();
    QVERIFY(rq != nullptr);
    rq->clear();
    QVERIFY(rq->isEmpty());

    Process p(10, 501, 2, 35, ProcessState::Ready);
    QString err;
    QVERIFY(m_service.enqueueProcess(p, &err));
    QCOMPARE(m_service.readyQueue()->size(), 1);

    Process dequeued;
    QVERIFY(m_service.dequeueProcess(&dequeued, &err));
    QCOMPARE(dequeued.flightId, 501);
    QVERIFY(m_service.readyQueue()->isEmpty());
}

void TestReadyQueueAndProcessStates::testServiceSyncReadyQueueFromDatabase()
{
    if (!m_dbConnected || m_testFlightId1 == 0) {
        QSKIP("Live MySQL database is offline; skipping database sync integration test.");
    }

    m_service.setTestSession(Session(1, QStringLiteral("atc_tester"), QStringLiteral("ATC Tester"), QStringLiteral("ATC")));

    // Clean scheduling_queue for test flights
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(db);
    q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (%1, %2);").arg(m_testFlightId1).arg(m_testFlightId2));

    // Enqueue m_testFlightId1 (should persist to DB and in-memory queue)
    QString err;
    Process p1;
    bool created = m_service.createProcessForFlight(m_testFlightId1, 60, &p1, &err);
    QVERIFY2(created, qPrintable(err));
    QVERIFY2(p1.isValid(&err), qPrintable(err));
    QCOMPARE(p1.processState(), ProcessState::Ready);

    // Clear in-memory queue and resync from DB
    m_service.readyQueue()->clear();
    QVERIFY(m_service.readyQueue()->isEmpty());

    bool synced = m_service.syncReadyQueueFromDatabase(&err);
    QVERIFY2(synced, qPrintable(err));
    QVERIFY(m_service.readyQueue()->containsFlight(m_testFlightId1));

    // Update state to Running via service
    bool ok = m_service.updateProcessState(p1.flightId, ProcessState::Running, &err);
    QVERIFY2(ok, qPrintable(err));

    // Should be removed from readyQueue
    QVERIFY(!m_service.readyQueue()->containsFlight(m_testFlightId1));

    // Update state to Completed via service
    ok = m_service.updateProcessState(p1.flightId, ProcessState::Completed, &err);
    QVERIFY2(ok, qPrintable(err));

    // Verify DB reflects Completed
    Process reloaded;
    QVERIFY(m_service.getProcessByFlightId(p1.flightId, reloaded, &err));
    QCOMPARE(reloaded.processState(), ProcessState::Completed);
}

void TestReadyQueueAndProcessStates::testServiceFlightDeletionRemovesFromReadyQueue()
{
    if (!m_dbConnected || m_testFlightId2 == 0) {
        QSKIP("Live MySQL database is offline; skipping flight deletion cleanup test.");
    }

    m_service.setTestSession(Session(1, QStringLiteral("atc_tester"), QStringLiteral("ATC Tester"), QStringLiteral("ATC")));

    QString err;
    // Enqueue flight 2
    Process p2;
    bool created = m_service.createProcessForFlight(m_testFlightId2, 60, &p2, &err);
    QVERIFY2(created, qPrintable(err));
    QVERIFY2(p2.isValid(&err), qPrintable(err));
    QVERIFY(m_service.readyQueue()->containsFlight(m_testFlightId2));

    // Delete flight 2 via service
    bool deleted = m_service.deleteFlight(m_testFlightId2);
    QVERIFY(deleted);

    // Ready queue must no longer contain the deleted flight
    QVERIFY(!m_service.readyQueue()->containsFlight(m_testFlightId2));
}

QTEST_MAIN(TestReadyQueueAndProcessStates)
#include "TestReadyQueueAndProcessStates.moc"
