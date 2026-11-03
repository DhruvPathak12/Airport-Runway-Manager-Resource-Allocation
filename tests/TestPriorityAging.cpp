#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>

#include "models/Process.h"
#include "models/Flight.h"
#include "models/Session.h"
#include "scheduler/ReadyQueue.h"
#include "scheduler/Scheduler.h"
#include "scheduler/AgingEngine.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"

using namespace Models;
using namespace SchedulerComponents;

class TestPriorityAging : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ── 1. Priority Calculation Tests ─────────────────────────────────────────
    void testBasePriority5Remains5BeforeAging();
    void testOneAgingLevelChangesPriority5To4();
    void testTwoAgingLevelsChangesPriority5To3();
    void testFourAgingLevelsChangesPriority5To1();
    void testPriorityNeverLessThan1();
    void testBasePriorityNeverModified();
    void testPriority1Remains1();

    // ── 2. Emergency Interaction Tests ────────────────────────────────────────
    void testEmergencyProcessRemainsEffectivePriority1();
    void testAgingDoesNotOverwriteEmergencyPriority();
    void testClearingEmergencyRestoresAgingAdjustedPriority();
    void testEmergencyDeclarationPreservesBasePriority();
    void testMultipleEmergencyProcessesOrderedDeterministically();

    // ── 3. Process State and Time Tracking Tests ──────────────────────────────
    void testReadyProcessesReceiveAging();
    void testRunningProcessesDoNotReceiveAging();
    void testBlockedProcessesDoNotReceiveAging();
    void testCompletedProcessesDoNotReceiveAging();
    void testLeavingReadyStopsWaitingPeriod();
    void testReturningToReadyStartsNewWaitingPeriod();
    void testDelayedTimerCallbacksDoNotCauseDuplicateAging();
    void testPriorityChangesCalculatedFromActualElapsedWaitingTime();

    // ── 4. Queue and Scheduler Integration Tests ──────────────────────────────
    void testQueueReorderedAfterAging();
    void testEmergencyProcessRemainsAheadOfNormalProcesses();
    void testTieBreakingRemainsDeterministic();
    void testNoDuplicateQueueEntriesCreated();
    void testRunningProcessNeverPreempted();
    void testSchedulerSelectsHighestEffectivePriorityReadyProcess();

    // ── 5. Service, Database, and RBAC Tests ──────────────────────────────────
    void testAgingIntervalValidation();
    void testStartAndStopBehavior();
    void testUnauthorizedConfigurationChangesRejected();
    void testDatabaseDisconnectionHandledSafely();
    void testBasePriorityRemainsUnchangedInDatabase();
    void testEffectivePriorityPersistsCorrectly();
    void testExistingDay16EmergencyTestsContinuePassing();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;
    int                m_testFlightId1 = 0;
    int                m_testFlightId2 = 0;
};

void TestPriorityAging::initTestCase()
{
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();

    if (!m_dbConnected) {
        qWarning() << "[TestPriorityAging] Live DB offline; running standalone unit tests only.";
        return;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'AGE-%');"));
        q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'AGE-%';"));

        m_service.setTestSession(Session(1, QStringLiteral("atc_tester"), QStringLiteral("ATC Tester"), QStringLiteral("ATC")));

        Flight f1;
        f1.flightNumber = QStringLiteral("AGE-101");
        f1.airline = QStringLiteral("SkyWay");
        f1.source = QStringLiteral("BOM");
        f1.destination = QStringLiteral("DEL");
        f1.flightType = QStringLiteral("Arrival");
        f1.scheduledTime = QDateTime::currentDateTime().addSecs(1200);
        f1.priority = 5;
        f1.operationalStatus = QStringLiteral("Scheduled");

        if (m_service.createFlight(f1)) {
            QList<Flight> flist = m_service.getAllFlights(QStringLiteral("AGE-101"));
            if (!flist.isEmpty()) {
                m_testFlightId1 = flist.first().id;
            }
        }

        Flight f2;
        f2.flightNumber = QStringLiteral("AGE-102");
        f2.airline = QStringLiteral("SkyWay");
        f2.source = QStringLiteral("DEL");
        f2.destination = QStringLiteral("BLR");
        f2.flightType = QStringLiteral("Departure");
        f2.scheduledTime = QDateTime::currentDateTime().addSecs(2400);
        f2.priority = 4;
        f2.operationalStatus = QStringLiteral("Scheduled");

        if (m_service.createFlight(f2)) {
            QList<Flight> flist = m_service.getAllFlights(QStringLiteral("AGE-102"));
            if (!flist.isEmpty()) {
                m_testFlightId2 = flist.first().id;
            }
        }
    }
}

void TestPriorityAging::cleanupTestCase()
{
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'AGE-%');"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'AGE-%';"));
        }
    }
    m_service.shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Priority Calculation Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestPriorityAging::testBasePriority5Remains5BeforeAging()
{
    Process p(1, 101, 5, 60, ProcessState::Ready);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    // 0 elapsed time
    QCOMPARE(p.basePriority, 5);
    QCOMPARE(p.effectivePriority, 5);
    QCOMPARE(p.agingLevel, 0);
    QCOMPARE(p.calculateAgingLevels(now, 30), 0);
    QCOMPARE(p.calculateEffectivePriority(now, 30), 5);
}

void TestPriorityAging::testOneAgingLevelChangesPriority5To4()
{
    Process p(1, 101, 5, 60, ProcessState::Ready);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    QDateTime t1 = now.addSecs(30); // 1 interval (30s)
    QCOMPARE(p.calculateAgingLevels(t1, 30), 1);
    QCOMPARE(p.calculateEffectivePriority(t1, 30), 4);

    bool changed = p.applyAging(t1, 30);
    QVERIFY(changed);
    QCOMPARE(p.effectivePriority, 4);
    QCOMPARE(p.agingLevel, 1);
    QCOMPARE(p.basePriority, 5); // Base priority remains 5
}

void TestPriorityAging::testTwoAgingLevelsChangesPriority5To3()
{
    Process p(1, 101, 5, 60, ProcessState::Ready);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    QDateTime t2 = now.addSecs(60); // 2 intervals (60s)
    QCOMPARE(p.calculateAgingLevels(t2, 30), 2);
    QCOMPARE(p.calculateEffectivePriority(t2, 30), 3);

    p.applyAging(t2, 30);
    QCOMPARE(p.effectivePriority, 3);
    QCOMPARE(p.agingLevel, 2);
    QCOMPARE(p.basePriority, 5);
}

void TestPriorityAging::testFourAgingLevelsChangesPriority5To1()
{
    Process p(1, 101, 5, 60, ProcessState::Ready);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    QDateTime t4 = now.addSecs(120); // 4 intervals (120s)
    QCOMPARE(p.calculateAgingLevels(t4, 30), 4);
    QCOMPARE(p.calculateEffectivePriority(t4, 30), 1);

    p.applyAging(t4, 30);
    QCOMPARE(p.effectivePriority, 1);
    QCOMPARE(p.agingLevel, 4);
    QCOMPARE(p.basePriority, 5);
}

void TestPriorityAging::testPriorityNeverLessThan1()
{
    Process p(1, 101, 5, 60, ProcessState::Ready);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    QDateTime t10 = now.addSecs(300); // 10 intervals (300s)
    QCOMPARE(p.calculateAgingLevels(t10, 30), 10);
    QCOMPARE(p.calculateEffectivePriority(t10, 30), 1); // Bounded at 1

    p.applyAging(t10, 30);
    QCOMPARE(p.effectivePriority, 1);
    QVERIFY(p.effectivePriority >= 1);
    QCOMPARE(p.basePriority, 5);
}

void TestPriorityAging::testBasePriorityNeverModified()
{
    Process p(1, 101, 4, 60, ProcessState::Ready);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    for (int sec = 30; sec <= 180; sec += 30) {
        p.applyAging(now.addSecs(sec), 30);
        QCOMPARE(p.basePriority, 4); // Always strictly 4
    }
}

void TestPriorityAging::testPriority1Remains1()
{
    Process p(1, 101, 1, 60, ProcessState::Ready);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    QCOMPARE(p.effectivePriority, 1);
    QDateTime t3 = now.addSecs(90);
    p.applyAging(t3, 30);
    QCOMPARE(p.effectivePriority, 1);
    QCOMPARE(p.basePriority, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Emergency Interaction Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestPriorityAging::testEmergencyProcessRemainsEffectivePriority1()
{
    Process p(1, 101, 5, 60, ProcessState::Ready);
    p.setEmergency(true);
    QCOMPARE(p.effectivePriority, 1);
    QCOMPARE(p.basePriority, 5);

    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    // Even with 0 elapsed time, emergency is 1
    QCOMPARE(p.calculateEffectivePriority(now, 30), 1);
}

void TestPriorityAging::testAgingDoesNotOverwriteEmergencyPriority()
{
    Process p(1, 101, 5, 60, ProcessState::Ready);
    p.setEmergency(true);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    // Elapsed 30s (1 interval)
    QDateTime t1 = now.addSecs(30);
    p.applyAging(t1, 30);
    QCOMPARE(p.effectivePriority, 1); // Remains 1, not overwritten by 4
    QCOMPARE(p.agingLevel, 1);
    QCOMPARE(p.basePriority, 5);
}

void TestPriorityAging::testClearingEmergencyRestoresAgingAdjustedPriority()
{
    Process p(1, 101, 5, 60, ProcessState::Ready);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    // Accrue 2 aging levels while waiting
    p.applyAging(now.addSecs(60), 30);
    QCOMPARE(p.agingLevel, 2);
    QCOMPARE(p.effectivePriority, 3); // 5 - 2 = 3

    // Emergency declared: elevates to 1
    p.setEmergency(true);
    QCOMPARE(p.effectivePriority, 1);

    // Emergency cleared: restores to aging-adjusted priority (3), NOT base (5)!
    p.setEmergency(false);
    QCOMPARE(p.effectivePriority, 3);
    QCOMPARE(p.basePriority, 5);
}

void TestPriorityAging::testEmergencyDeclarationPreservesBasePriority()
{
    Process p(1, 101, 4, 60, ProcessState::Ready);
    p.setEmergency(true);
    QCOMPARE(p.basePriority, 4);
    p.setEmergency(false);
    QCOMPARE(p.basePriority, 4);
}

void TestPriorityAging::testMultipleEmergencyProcessesOrderedDeterministically()
{
    Scheduler scheduler;
    QDateTime now = QDateTime::currentDateTime();

    Process e1(1, 101, 4, 60, ProcessState::Ready);
    e1.arrivalTime = now;
    e1.setEmergency(true);

    Process e2(2, 102, 5, 60, ProcessState::Ready);
    e2.arrivalTime = now.addSecs(10);
    e2.setEmergency(true);

    scheduler.readyQueue().enqueue(e2);
    scheduler.readyQueue().enqueue(e1);

    Models::Process selected;
    QVERIFY(scheduler.selectNextProcess(&selected));
    QCOMPARE(selected.flightId, 101); // e1 arrived earlier (now < now + 10s)
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Process State and Time Tracking Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestPriorityAging::testReadyProcessesReceiveAging()
{
    Process p(1, 101, 4, 60, ProcessState::Ready);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    bool changed = p.applyAging(now.addSecs(30), 30);
    QVERIFY(changed);
    QCOMPARE(p.effectivePriority, 3);
}

void TestPriorityAging::testRunningProcessesDoNotReceiveAging()
{
    Process p(1, 101, 4, 60, ProcessState::Running);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    bool changed = p.applyAging(now.addSecs(60), 30);
    QVERIFY(!changed);
    QCOMPARE(p.effectivePriority, 4); // Did not change
    QCOMPARE(p.agingLevel, 0);
}

void TestPriorityAging::testBlockedProcessesDoNotReceiveAging()
{
    Process p(1, 101, 4, 60, ProcessState::Blocked);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    bool changed = p.applyAging(now.addSecs(60), 30);
    QVERIFY(!changed);
    QCOMPARE(p.effectivePriority, 4);
}

void TestPriorityAging::testCompletedProcessesDoNotReceiveAging()
{
    Process p(1, 101, 4, 60, ProcessState::Completed);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    bool changed = p.applyAging(now.addSecs(60), 30);
    QVERIFY(!changed);
    QCOMPARE(p.effectivePriority, 4);
}

void TestPriorityAging::testLeavingReadyStopsWaitingPeriod()
{
    Process p(1, 101, 3, 60, ProcessState::Ready);
    QVERIFY(p.readySince.isValid());

    // Transition to Running ends continuous ready waiting period
    QVERIFY(p.transitionTo(ProcessState::Running));
    QVERIFY(!p.readySince.isValid());
}

void TestPriorityAging::testReturningToReadyStartsNewWaitingPeriod()
{
    Process p(1, 101, 4, 60, ProcessState::Ready);
    QDateTime t0 = QDateTime::currentDateTime();
    p.readySince = t0;

    // Dispatched to Running
    p.transitionTo(ProcessState::Running);

    // Blocked
    p.transitionTo(ProcessState::Blocked);

    // Later unblocks and returns to Ready at t1
    QDateTime t1 = t0.addSecs(100);
    p.transitionTo(ProcessState::Ready, t1);

    QVERIFY(p.readySince.isValid());
    QCOMPARE(p.readySince, t1);
    QCOMPARE(p.agingLevel, 0);
    QCOMPARE(p.effectivePriority, 4); // Reset to base priority at start of new period
}

void TestPriorityAging::testDelayedTimerCallbacksDoNotCauseDuplicateAging()
{
    Process p(1, 101, 5, 60, ProcessState::Ready);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    QDateTime t1 = now.addSecs(30);

    // Apply aging 5 times at the exact same timestamp t1
    for (int i = 0; i < 5; ++i) {
        p.applyAging(t1, 30);
    }

    // Must be exactly 1 aging level, not 5!
    QCOMPARE(p.agingLevel, 1);
    QCOMPARE(p.effectivePriority, 4);
}

void TestPriorityAging::testPriorityChangesCalculatedFromActualElapsedWaitingTime()
{
    Process p(1, 101, 5, 60, ProcessState::Ready);
    QDateTime now = QDateTime::currentDateTime();
    p.readySince = now;

    // 89 seconds: floor(89 / 30) = 2
    p.applyAging(now.addSecs(89), 30);
    QCOMPARE(p.agingLevel, 2);
    QCOMPARE(p.effectivePriority, 3);

    // 90 seconds: floor(90 / 30) = 3
    p.applyAging(now.addSecs(90), 30);
    QCOMPARE(p.agingLevel, 3);
    QCOMPARE(p.effectivePriority, 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Queue and Scheduler Integration Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestPriorityAging::testQueueReorderedAfterAging()
{
    ReadyQueue queue;
    QDateTime now = QDateTime::currentDateTime();

    Process pA(1, 101, 2, 60, ProcessState::Ready); // High priority, just arrived
    pA.arrivalTime = now;
    pA.readySince = now;

    Process pB(2, 102, 5, 60, ProcessState::Ready); // Low priority, arrived 120s ago
    pB.arrivalTime = now.addSecs(-120);
    pB.readySince = now.addSecs(-120);

    queue.enqueue(pA);
    queue.enqueue(pB);

    // Initially pA (eff 2) is ahead of pB (eff 5)
    QCOMPARE(queue.peek()->flightId, 101);

    // Evaluate aging at time 'now':
    // pA elapsed 0s -> eff 2
    // pB elapsed 120s -> 4 intervals -> eff 1 (5 - 4 = 1)
    int updated = 0;
    queue.evaluateAging(30, now, &updated);
    QCOMPARE(updated, 1);

    // After aging, pB (eff 1) must move ahead of pA (eff 2)!
    QCOMPARE(queue.peek()->flightId, 102);
}

void TestPriorityAging::testEmergencyProcessRemainsAheadOfNormalProcesses()
{
    Scheduler scheduler;
    QDateTime now = QDateTime::currentDateTime();

    Process pNormal(1, 101, 5, 60, ProcessState::Ready);
    pNormal.arrivalTime = now.addSecs(-120);
    pNormal.readySince = now.addSecs(-120); // Aged to eff 1 (4 intervals)

    Process pEmerg(2, 102, 3, 60, ProcessState::Ready);
    pEmerg.arrivalTime = now.addSecs(-200); // Arrived earlier
    pEmerg.setEmergency(true); // eff 1

    scheduler.readyQueue().enqueue(pNormal);
    scheduler.readyQueue().enqueue(pEmerg);

    int updated = 0;
    scheduler.evaluateAging(now, &updated);

    Models::Process selected;
    QVERIFY(scheduler.selectNextProcess(&selected));
    // Both eff 1: pEmerg arrived earlier (-200 < -120), so pEmerg wins
    QCOMPARE(selected.flightId, 102);
}

void TestPriorityAging::testTieBreakingRemainsDeterministic()
{
    ReadyQueue queue;
    QDateTime now = QDateTime::currentDateTime();

    Process p1(1, 201, 5, 60, ProcessState::Ready);
    p1.arrivalTime = now.addSecs(-60);
    p1.readySince = now.addSecs(-60);

    Process p2(2, 202, 5, 60, ProcessState::Ready);
    p2.arrivalTime = now.addSecs(-60);
    p2.readySince = now.addSecs(-60);

    queue.enqueue(p2);
    queue.enqueue(p1);

    int updated = 0;
    queue.evaluateAging(30, now, &updated);

    // Both aged to eff 3 with identical arrival time. Tie-breaking by flightId ASC: 201 < 202
    QCOMPARE(queue.peek()->flightId, 201);
}

void TestPriorityAging::testNoDuplicateQueueEntriesCreated()
{
    ReadyQueue queue;
    QDateTime now = QDateTime::currentDateTime();

    queue.enqueue(Process(1, 101, 4, 60, ProcessState::Ready));
    queue.enqueue(Process(2, 102, 5, 60, ProcessState::Ready));
    QCOMPARE(queue.size(), 2);

    int updated = 0;
    queue.evaluateAging(30, now.addSecs(60), &updated);
    QCOMPARE(queue.size(), 2); // Still exactly 2
}

void TestPriorityAging::testRunningProcessNeverPreempted()
{
    Scheduler scheduler;
    QDateTime now = QDateTime::currentDateTime();

    Process pRun(1, 101, 4, 60, ProcessState::Ready);
    scheduler.readyQueue().enqueue(pRun);
    Scheduler::DispatchResult res = scheduler.dispatchNextProcess();
    QVERIFY(res.success);
    QVERIFY(scheduler.hasRunningProcess());

    // Another process in Ready ages to priority 1
    Process pWait(2, 102, 5, 60, ProcessState::Ready);
    pWait.readySince = now.addSecs(-120);
    scheduler.readyQueue().enqueue(pWait);

    int updated = 0;
    scheduler.evaluateAging(now, &updated);

    // Dispatch attempt must fail because scheduler is non-preemptive
    Scheduler::DispatchResult res2 = scheduler.dispatchNextProcess();
    QVERIFY(!res2.success);
    QCOMPARE(scheduler.runningProcess().flightId, 101);
}

void TestPriorityAging::testSchedulerSelectsHighestEffectivePriorityReadyProcess()
{
    Scheduler scheduler;
    QDateTime now = QDateTime::currentDateTime();

    Process pLow(1, 101, 5, 60, ProcessState::Ready);
    pLow.readySince = now.addSecs(-90); // 3 intervals -> eff 2

    Process pMid(2, 102, 3, 60, ProcessState::Ready);
    pMid.readySince = now; // 0 intervals -> eff 3

    scheduler.readyQueue().enqueue(pMid);
    scheduler.readyQueue().enqueue(pLow);

    int updated = 0;
    scheduler.evaluateAging(now, &updated);

    Models::Process candidate;
    QVERIFY(scheduler.selectNextProcess(&candidate));
    QCOMPARE(candidate.flightId, 101); // pLow aged to eff 2, beats pMid (eff 3)
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. Service, Database, and RBAC Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestPriorityAging::testAgingIntervalValidation()
{
    m_service.setTestSession(Session(1, QStringLiteral("admin_test"), QStringLiteral("Admin User"), QStringLiteral("Admin")));

    QString err;
    QVERIFY(m_service.setAgingInterval(15, &err));
    QCOMPARE(m_service.agingInterval(), 15);

    QVERIFY(!m_service.setAgingInterval(0, &err));
    QVERIFY(!m_service.setAgingInterval(-5, &err));
    QVERIFY(!m_service.setAgingInterval(100000, &err));

    // Restore default
    m_service.setAgingInterval(30);
    QCOMPARE(m_service.agingInterval(), 30);
}

void TestPriorityAging::testStartAndStopBehavior()
{
    m_service.setTestSession(Session(1, QStringLiteral("admin_test"), QStringLiteral("Admin User"), QStringLiteral("Admin")));

    QVERIFY(!m_service.isAgingRunning());
    QVERIFY(m_service.startAging());
    QVERIFY(m_service.isAgingRunning());

    QVERIFY(m_service.stopAging());
    QVERIFY(!m_service.isAgingRunning());
}

void TestPriorityAging::testUnauthorizedConfigurationChangesRejected()
{
    // 1. Unauthenticated user
    m_service.logout();
    QString err;
    QVERIFY(!m_service.startAging(&err));
    QVERIFY(err.contains(QStringLiteral("Authentication required"), Qt::CaseInsensitive));
    QVERIFY(!m_service.setAgingInterval(20, &err));
    QVERIFY(m_service.evaluateAging(QDateTime::currentDateTime(), nullptr, &err) < 0);
    QVERIFY(err.contains(QStringLiteral("Authentication required"), Qt::CaseInsensitive));

    // 2. Airport Operator role (denied scheduling administration)
    m_service.setTestSession(Session(2, QStringLiteral("operator_user"), QStringLiteral("Operator"), QStringLiteral("Airport Operator")));
    QVERIFY(!m_service.startAging(&err));
    QVERIFY(err.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));
    QVERIFY(!m_service.setAgingInterval(20, &err));
    QVERIFY(err.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));
    QVERIFY(!m_service.stopAging(&err));
    QVERIFY(err.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));
    QVERIFY(m_service.evaluateAging(QDateTime::currentDateTime(), nullptr, &err) < 0);
    QVERIFY(err.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));
}

void TestPriorityAging::testDatabaseDisconnectionHandledSafely()
{
    // Simulation / in-memory operation operates safely even when database is disconnected
    m_service.setTestSession(Session(1, QStringLiteral("atc_test"), QStringLiteral("ATC"), QStringLiteral("ATC")));

    int updated = 0;
    QString err;
    int evaluated = m_service.evaluateAging(QDateTime::currentDateTime(), &updated, &err);
    QVERIFY(evaluated >= 0); // Safe return without crashing
}

void TestPriorityAging::testBasePriorityRemainsUnchangedInDatabase()
{
    if (!m_dbConnected || m_testFlightId1 <= 0) {
        QSKIP("Live MySQL database not connected; skipping live DB test.");
    }

    m_service.setTestSession(Session(1, QStringLiteral("atc_test"), QStringLiteral("ATC"), QStringLiteral("ATC")));

    // Flight 1 has base priority 5
    QDateTime now = QDateTime::currentDateTime();
    Process p(0, m_testFlightId1, 5, 60, ProcessState::Ready);
    p.readySince = now.addSecs(-60); // 2 intervals -> eff 3

    m_service.createProcess(p);

    int updated = 0;
    m_service.evaluateAging(now, &updated);

    // Query MySQL scheduling_queue
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT base_priority, effective_priority FROM scheduling_queue WHERE flight_id = :fid;"));
    q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
    QVERIFY(q.exec() && q.next());

    QCOMPARE(q.value(0).toInt(), 5); // Base priority strictly preserved as 5!
    QCOMPARE(q.value(1).toInt(), 3); // Effective priority updated to 3
}

void TestPriorityAging::testEffectivePriorityPersistsCorrectly()
{
    if (!m_dbConnected || m_testFlightId2 <= 0) {
        QSKIP("Live MySQL database not connected; skipping live DB test.");
    }

    m_service.setTestSession(Session(1, QStringLiteral("atc_test"), QStringLiteral("ATC"), QStringLiteral("ATC")));

    QDateTime now = QDateTime::currentDateTime();
    Process p(0, m_testFlightId2, 4, 60, ProcessState::Ready);
    p.readySince = now.addSecs(-90); // 3 intervals -> eff 1 (4 - 3 = 1)

    m_service.createProcess(p);

    int updated = 0;
    m_service.evaluateAging(now, &updated);

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT effective_priority, aging_level FROM scheduling_queue WHERE flight_id = :fid;"));
    q.bindValue(QStringLiteral(":fid"), m_testFlightId2);
    QVERIFY(q.exec() && q.next());

    QCOMPARE(q.value(0).toInt(), 1); // Persisted effective priority 1
    QCOMPARE(q.value(1).toInt(), 3); // Persisted aging level 3
}

void TestPriorityAging::testExistingDay16EmergencyTestsContinuePassing()
{
    m_service.setTestSession(Session(1, QStringLiteral("atc_test"), QStringLiteral("ATC"), QStringLiteral("ATC")));

    Process p(1, 301, 3, 60, ProcessState::Ready);
    p.setEmergency(true);
    QCOMPARE(p.effectivePriority, 1);
    QCOMPARE(p.basePriority, 3);

    p.setEmergency(false);
    QCOMPARE(p.effectivePriority, 3);
    QCOMPARE(p.basePriority, 3);
}

QTEST_MAIN(TestPriorityAging)
#include "TestPriorityAging.moc"

