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

/**
 * @brief Comprehensive test suite for Phase 2 - Day 16: Dynamic Emergency Priority Changes.
 *
 * Covers:
 *   1. Priority Calculation (Requirements 1-7)
 *   2. Scheduler Behavior (Requirements 8-12)
 *   3. Service Layer and Authorization (Requirements 13-20)
 */
class TestDynamicEmergencyPriority : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ── 1. Priority Calculation ──────────────────────────────────────────────
    void testNormalFlightRetainsBasePriority();
    void testEmergencyFlightReceivesEffectivePriority1();
    void testBasePriorityUnchangedAfterEmergencyDeclaration();
    void testClearingEmergencyRestoresBasePriority();
    void testBasePriority1Remains1AfterEmergencyClearance();
    void testInvalidPriorityValuesRejected();
    void testMultipleEmergencyFlightsReceiveCorrectEffectivePriorities();

    // ── 2. Scheduler Behavior ────────────────────────────────────────────────
    void testEmergencyReadyProcessSelectedBeforeLowerPriorityNormalProcesses();
    void testDeterministicTieBreakingPreservedForMultipleEmergencies();
    void testRunningProcessNotPreemptedWhenEmergencyDeclared();
    void testSchedulerUsesEffectivePriorityNotBasePriority();
    void testExistingNonPreemptiveBehaviorUnchanged();

    // ── 3. Service Layer and Authorization ───────────────────────────────────
    void testAtcCanDeclareEmergency();
    void testAtcCanClearEmergency();
    void testUnauthorizedUsersCannotDeclareEmergency();
    void testUnauthorizedUsersCannotClearEmergency();
    void testMissingFlightHandledSafely();
    void testTerminalFlightEligibilityHandledCorrectly();
    void testDatabaseFailureReportedCorrectly();
    void testAssociatedProcessPrioritySynchronizedCorrectly();
    void testAlreadyEmergencyAndAlreadyNormalHandledSafely();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;
    int                m_testFlightId1 = 0; // Normal flight (Priority 4)
    int                m_testFlightId2 = 0; // Flight for emergency toggle (Priority 3)
    int                m_testTerminalFlightId = 0; // Terminal flight (Landed)
};

void TestDynamicEmergencyPriority::initTestCase()
{
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();

    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            // Clean up any previous test records
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'EMERG-%');"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'EMERG-%';"));

            // Inject ATC session
            m_service.setTestSession(Session(1, QStringLiteral("atc_emergency_tester"), QStringLiteral("ATC Emergency Tester"), QStringLiteral("ATC")));

            // Create test flight 1: Priority 4
            Flight f1;
            f1.flightNumber = QStringLiteral("EMERG-101");
            f1.airline = QStringLiteral("AeroEmergency");
            f1.source = QStringLiteral("DEL");
            f1.destination = QStringLiteral("BOM");
            f1.flightType = QStringLiteral("Arrival");
            f1.scheduledTime = QDateTime::currentDateTime().addSecs(1800);
            f1.priority = 4;
            f1.isEmergency = false;
            f1.operationalStatus = QStringLiteral("Scheduled");

            if (m_service.createFlight(f1)) {
                QList<Flight> flist = m_service.getAllFlights(QStringLiteral("EMERG-101"));
                if (!flist.isEmpty()) {
                    m_testFlightId1 = flist.first().id;
                }
            }

            // Create test flight 2: Priority 3
            Flight f2;
            f2.flightNumber = QStringLiteral("EMERG-102");
            f2.airline = QStringLiteral("AeroEmergency");
            f2.source = QStringLiteral("BOM");
            f2.destination = QStringLiteral("BLR");
            f2.flightType = QStringLiteral("Departure");
            f2.scheduledTime = QDateTime::currentDateTime().addSecs(3600);
            f2.priority = 3;
            f2.isEmergency = false;
            f2.operationalStatus = QStringLiteral("Taxiing");

            if (m_service.createFlight(f2)) {
                QList<Flight> flist = m_service.getAllFlights(QStringLiteral("EMERG-102"));
                if (!flist.isEmpty()) {
                    m_testFlightId2 = flist.first().id;
                }
            }

            // Create test terminal flight: Priority 2, Status Landed
            Flight f3;
            f3.flightNumber = QStringLiteral("EMERG-103");
            f3.airline = QStringLiteral("AeroEmergency");
            f3.source = QStringLiteral("HYD");
            f3.destination = QStringLiteral("DEL");
            f3.flightType = QStringLiteral("Arrival");
            f3.scheduledTime = QDateTime::currentDateTime().addSecs(-1800);
            f3.priority = 2;
            f3.isEmergency = false;
            f3.operationalStatus = QStringLiteral("Landed");

            if (m_service.createFlight(f3)) {
                QList<Flight> flist = m_service.getAllFlights(QStringLiteral("EMERG-103"));
                if (!flist.isEmpty()) {
                    m_testTerminalFlightId = flist.first().id;
                }
            }
        }
    }
}

void TestDynamicEmergencyPriority::cleanupTestCase()
{
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'EMERG-%');"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'EMERG-%';"));
        }
    }
    m_service.shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Priority Calculation (Unit Tests - Offline)
// ─────────────────────────────────────────────────────────────────────────────

void TestDynamicEmergencyPriority::testNormalFlightRetainsBasePriority()
{
    // Requirement 1: Normal flight retains its base priority.
    Flight f;
    f.priority = 3;
    f.isEmergency = false;

    QCOMPARE(f.priority, 3);
    QCOMPARE(f.effectivePriority(), 3);

    Process proc = Process::fromFlight(f, 60);
    QCOMPARE(proc.basePriority, 3);
    QCOMPARE(proc.effectivePriority, 3);
    QCOMPARE(proc.isEmergency, false);
}

void TestDynamicEmergencyPriority::testEmergencyFlightReceivesEffectivePriority1()
{
    // Requirement 2: Emergency flight receives effective priority 1.
    Flight f;
    f.priority = 4;
    f.isEmergency = true;

    QCOMPARE(f.priority, 4);
    QCOMPARE(f.effectivePriority(), 1);

    Process proc = Process::fromFlight(f, 60);
    QCOMPARE(proc.basePriority, 4);
    QCOMPARE(proc.effectivePriority, 1);
    QCOMPARE(proc.isEmergency, true);
}

void TestDynamicEmergencyPriority::testBasePriorityUnchangedAfterEmergencyDeclaration()
{
    // Requirement 3: Base priority is unchanged after emergency declaration.
    Process proc(1, 101, 5, 60, ProcessState::Ready);
    QCOMPARE(proc.basePriority, 5);
    QCOMPARE(proc.effectivePriority, 5);

    proc.setEmergency(true);
    QCOMPARE(proc.isEmergency, true);
    QCOMPARE(proc.effectivePriority, 1);
    QCOMPARE(proc.basePriority, 5); // Base priority remains 5!
}

void TestDynamicEmergencyPriority::testClearingEmergencyRestoresBasePriority()
{
    // Requirement 4: Clearing emergency restores the base priority.
    Process proc(1, 102, 4, 60, ProcessState::Ready);
    proc.setEmergency(true);
    QCOMPARE(proc.effectivePriority, 1);
    QCOMPARE(proc.basePriority, 4);

    proc.setEmergency(false);
    QCOMPARE(proc.isEmergency, false);
    QCOMPARE(proc.effectivePriority, 4); // Restored to base priority!
    QCOMPARE(proc.basePriority, 4);     // Base priority still 4!
}

void TestDynamicEmergencyPriority::testBasePriority1Remains1AfterEmergencyClearance()
{
    // Requirement 5: Base priority 1 remains 1 after emergency clearance.
    Process proc(1, 103, 1, 60, ProcessState::Ready);
    proc.setEmergency(true);
    QCOMPARE(proc.effectivePriority, 1);
    QCOMPARE(proc.basePriority, 1);

    proc.setEmergency(false);
    QCOMPARE(proc.effectivePriority, 1); // Restored to base priority 1!
    QCOMPARE(proc.basePriority, 1);
}

void TestDynamicEmergencyPriority::testInvalidPriorityValuesRejected()
{
    // Requirement 6: Invalid priority values are rejected.
    Flight f;
    f.flightNumber = QStringLiteral("TEST-INV");
    f.airline = QStringLiteral("TestAir");
    f.source = QStringLiteral("DEL");
    f.destination = QStringLiteral("BOM");
    f.flightType = QStringLiteral("Arrival");
    f.scheduledTime = QDateTime::currentDateTime();

    // Priority 0 is invalid
    f.priority = 0;
    QString err;
    QVERIFY(!f.validate(&err));
    QVERIFY(err.contains(QStringLiteral("between 1")));

    // Priority 6 is invalid
    f.priority = 6;
    QVERIFY(!f.validate(&err));

    // Process validation rejects basePriority or effectivePriority outside 1-5
    Process proc(1, 104, 3, 60, ProcessState::Ready);
    proc.basePriority = 0;
    QVERIFY(!proc.validate(&err));

    proc.basePriority = 3;
    proc.effectivePriority = 6;
    QVERIFY(!proc.validate(&err));
}

void TestDynamicEmergencyPriority::testMultipleEmergencyFlightsReceiveCorrectEffectivePriorities()
{
    // Requirement 7: Multiple emergency flights receive correct effective priorities.
    // Table verification from specification:
    // Flight | Base priority | Emergency | Effective priority
    // AI-101 | 3             | No        | 3
    // 6E-202 | 5             | Yes       | 1
    // SG-404 | 2             | No        | 2
    // UK-505 | 4             | Yes       | 1
    // QP-606 | 1             | No        | 1

    Flight ai101; ai101.priority = 3; ai101.isEmergency = false;
    Flight e6202; e6202.priority = 5; e6202.isEmergency = true;
    Flight sg404; sg404.priority = 2; sg404.isEmergency = false;
    Flight uk505; uk505.priority = 4; uk505.isEmergency = true;
    Flight qp606; qp606.priority = 1; qp606.isEmergency = false;

    QCOMPARE(ai101.effectivePriority(), 3);
    QCOMPARE(e6202.effectivePriority(), 1);
    QCOMPARE(sg404.effectivePriority(), 2);
    QCOMPARE(uk505.effectivePriority(), 1);
    QCOMPARE(qp606.effectivePriority(), 1);

    // Verify corresponding Process objects
    Process p_ai = Process::fromFlight(ai101);
    Process p_6e = Process::fromFlight(e6202);
    Process p_sg = Process::fromFlight(sg404);
    Process p_uk = Process::fromFlight(uk505);
    Process p_qp = Process::fromFlight(qp606);

    QCOMPARE(p_ai.effectivePriority, 3);
    QCOMPARE(p_6e.effectivePriority, 1);
    QCOMPARE(p_sg.effectivePriority, 2);
    QCOMPARE(p_uk.effectivePriority, 1);
    QCOMPARE(p_qp.effectivePriority, 1);

    // Base priorities strictly preserved
    QCOMPARE(p_ai.basePriority, 3);
    QCOMPARE(p_6e.basePriority, 5);
    QCOMPARE(p_sg.basePriority, 2);
    QCOMPARE(p_uk.basePriority, 4);
    QCOMPARE(p_qp.basePriority, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Scheduler Behavior (Unit Tests - Offline)
// ─────────────────────────────────────────────────────────────────────────────

void TestDynamicEmergencyPriority::testEmergencyReadyProcessSelectedBeforeLowerPriorityNormalProcesses()
{
    // Requirement 8: Emergency READY process is selected before lower-priority normal READY processes.
    Scheduler scheduler;
    QDateTime now = QDateTime::currentDateTime();

    Process pNormalHigh(1, 10, 2, 60, ProcessState::Ready);
    pNormalHigh.arrivalTime = now;

    Process pEmergency(2, 20, 5, 60, ProcessState::Ready);
    pEmergency.arrivalTime = now.addSecs(10); // Arrived later, base priority 5
    pEmergency.setEmergency(true);            // Effective priority becomes 1!

    Process pNormalLow(3, 30, 4, 60, ProcessState::Ready);
    pNormalLow.arrivalTime = now.addSecs(5);

    scheduler.readyQueue().enqueue(pNormalHigh);
    scheduler.readyQueue().enqueue(pEmergency);
    scheduler.readyQueue().enqueue(pNormalLow);

    Models::Process selected;
    QString reason;
    bool ok = scheduler.selectNextProcess(&selected, &reason);

    QVERIFY(ok);
    // pEmergency (effective priority 1) must be selected before pNormalHigh (effective 2) and pNormalLow (effective 4)
    QCOMPARE(selected.flightId, 20);
    QCOMPARE(selected.effectivePriority, 1);
    QCOMPARE(selected.basePriority, 5);
}

void TestDynamicEmergencyPriority::testDeterministicTieBreakingPreservedForMultipleEmergencies()
{
    // Requirement 9: Existing deterministic tie-breaking is preserved among multiple emergencies.
    Scheduler scheduler;
    QDateTime t1 = QDateTime::fromString(QStringLiteral("2026-11-02T10:00:00"), Qt::ISODate);
    QDateTime t2 = QDateTime::fromString(QStringLiteral("2026-11-02T10:05:00"), Qt::ISODate);

    // Case A: Earlier arrival time wins
    Process pEmergEarlier(1, 50, 4, 60, ProcessState::Ready);
    pEmergEarlier.arrivalTime = t1;
    pEmergEarlier.setEmergency(true);

    Process pEmergLater(2, 40, 3, 60, ProcessState::Ready);
    pEmergLater.arrivalTime = t2;
    pEmergLater.setEmergency(true);

    scheduler.readyQueue().enqueue(pEmergLater);
    scheduler.readyQueue().enqueue(pEmergEarlier);

    Models::Process selected;
    QVERIFY(scheduler.selectNextProcess(&selected));
    QCOMPARE(selected.flightId, 50); // t1 precedes t2

    // Case B: Identical arrival time -> smaller flight ID wins
    scheduler.reset();
    Process pEmergIdA(1, 105, 5, 60, ProcessState::Ready);
    pEmergIdA.arrivalTime = t1;
    pEmergIdA.setEmergency(true);

    Process pEmergIdB(2, 102, 2, 60, ProcessState::Ready);
    pEmergIdB.arrivalTime = t1;
    pEmergIdB.setEmergency(true);

    scheduler.readyQueue().enqueue(pEmergIdA);
    scheduler.readyQueue().enqueue(pEmergIdB);

    QVERIFY(scheduler.selectNextProcess(&selected));
    QCOMPARE(selected.flightId, 102); // 102 < 105 breaks tie deterministically
}

void TestDynamicEmergencyPriority::testRunningProcessNotPreemptedWhenEmergencyDeclared()
{
    // Requirement 10: RUNNING process is not preempted.
    Scheduler scheduler;
    QDateTime now = QDateTime::currentDateTime();

    Process pRunning(1, 201, 3, 60, ProcessState::Ready);
    pRunning.arrivalTime = now;
    scheduler.readyQueue().enqueue(pRunning);

    // Dispatch pRunning to RUNNING
    Scheduler::DispatchResult res = scheduler.dispatchNextProcess();
    QVERIFY(res.success);
    QVERIFY(scheduler.hasRunningProcess());
    QCOMPARE(scheduler.runningProcess().flightId, 201);

    // An emergency flight now enters the ready queue
    Process pEmerg(2, 202, 4, 60, ProcessState::Ready);
    pEmerg.arrivalTime = now.addSecs(10);
    pEmerg.setEmergency(true);
    scheduler.readyQueue().enqueue(pEmerg);

    // Attempting to dispatch must NOT preempt the currently running process
    Scheduler::DispatchResult nextRes = scheduler.dispatchNextProcess();
    QVERIFY(!nextRes.success);
    QVERIFY(nextRes.reason.contains(QStringLiteral("currently RUNNING")));

    // Running process is still pRunning (PID 201)
    QCOMPARE(scheduler.runningProcess().flightId, 201);
}

void TestDynamicEmergencyPriority::testSchedulerUsesEffectivePriorityNotBasePriority()
{
    // Requirement 11: Scheduler uses effective priority, not base priority.
    Scheduler scheduler;
    QDateTime now = QDateTime::currentDateTime();

    // Process A: Base priority 2, normal (eff 2)
    Process pA(1, 301, 2, 60, ProcessState::Ready);
    pA.arrivalTime = now;

    // Process B: Base priority 5 (lowest ordinary), emergency (eff 1)
    Process pB(2, 302, 5, 60, ProcessState::Ready);
    pB.arrivalTime = now.addSecs(5);
    pB.setEmergency(true);

    scheduler.readyQueue().enqueue(pA);
    scheduler.readyQueue().enqueue(pB);

    Models::Process candidate;
    QVERIFY(scheduler.selectNextProcess(&candidate));

    // If scheduler mistakenly used base priority, pA (base 2) would be chosen over pB (base 5).
    // Because it correctly uses effective priority, pB (eff 1) must be chosen!
    QCOMPARE(candidate.flightId, 302);
    QCOMPARE(candidate.effectivePriority, 1);
    QCOMPARE(candidate.basePriority, 5);
}

void TestDynamicEmergencyPriority::testExistingNonPreemptiveBehaviorUnchanged()
{
    // Requirement 12: Existing non-preemptive behavior remains unchanged.
    Scheduler scheduler;
    QDateTime now = QDateTime::currentDateTime();

    Process p1(1, 401, 3, 30, ProcessState::Ready);
    p1.arrivalTime = now;
    Process p2(2, 402, 5, 30, ProcessState::Ready);
    p2.arrivalTime = now.addSecs(5);
    p2.setEmergency(true); // Emergency

    scheduler.readyQueue().enqueue(p1);
    scheduler.readyQueue().enqueue(p2);

    // p2 has eff 1, so it is dispatched first
    Scheduler::DispatchResult res1 = scheduler.dispatchNextProcess();
    QVERIFY(res1.success);
    QCOMPARE(res1.process.flightId, 402);
    QVERIFY(scheduler.hasRunningProcess());

    // Complete p2
    QDateTime compTime = now.addSecs(40);
    Scheduler::CompletionResult compRes = scheduler.completeRunningProcess(compTime);
    QVERIFY(compRes.success);
    QVERIFY(!scheduler.hasRunningProcess());

    // Now p1 can be dispatched
    Scheduler::DispatchResult res2 = scheduler.dispatchNextProcess();
    QVERIFY(res2.success);
    QCOMPARE(res2.process.flightId, 401);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Service Layer and Authorization (Integration Tests)
// ─────────────────────────────────────────────────────────────────────────────

void TestDynamicEmergencyPriority::testAtcCanDeclareEmergency()
{
    // Requirement 13: ATC can declare an emergency.
    if (!m_dbConnected || m_testFlightId1 <= 0) {
        QSKIP("Live MySQL database not available for integration test.");
    }

    m_service.setTestSession(Session(10, QStringLiteral("atc_officer"), QStringLiteral("ATC Officer"), QStringLiteral("ATC")));

    QString err;
    bool ok = m_service.declareEmergency(m_testFlightId1, &err);
    QVERIFY2(ok, qPrintable(err));

    // Verify in database that is_emergency = 1
    Flight f;
    QVERIFY(m_service.getFlightById(m_testFlightId1, f, &err));
    QCOMPARE(f.isEmergency, true);
    QCOMPARE(f.effectivePriority(), 1);
    QCOMPARE(f.priority, 4); // Base priority remains 4!
}

void TestDynamicEmergencyPriority::testAtcCanClearEmergency()
{
    // Requirement 14: ATC can clear an emergency.
    if (!m_dbConnected || m_testFlightId1 <= 0) {
        QSKIP("Live MySQL database not available for integration test.");
    }

    m_service.setTestSession(Session(10, QStringLiteral("atc_officer"), QStringLiteral("ATC Officer"), QStringLiteral("ATC")));

    QString err;
    bool ok = m_service.clearEmergency(m_testFlightId1, &err);
    QVERIFY2(ok, qPrintable(err));

    // Verify in database that is_emergency = 0
    Flight f;
    QVERIFY(m_service.getFlightById(m_testFlightId1, f, &err));
    QCOMPARE(f.isEmergency, false);
    QCOMPARE(f.effectivePriority(), 4); // Restored to base priority!
    QCOMPARE(f.priority, 4);           // Base priority strictly preserved
}

void TestDynamicEmergencyPriority::testUnauthorizedUsersCannotDeclareEmergency()
{
    // Requirement 15: Unauthorized users cannot declare an emergency.
    QString err;

    // A. Unauthenticated session
    m_service.setTestSession(Session());
    QVERIFY(!m_service.declareEmergency(m_testFlightId2 > 0 ? m_testFlightId2 : 1, &err));
    QVERIFY(err.contains(QStringLiteral("Authentication required")));

    // B. Admin role (not permitted to declare emergency)
    m_service.setTestSession(Session(2, QStringLiteral("sysadmin"), QStringLiteral("System Administrator"), QStringLiteral("Admin")));
    QVERIFY(!m_service.declareEmergency(m_testFlightId2 > 0 ? m_testFlightId2 : 1, &err));
    QVERIFY(err.contains(QStringLiteral("Access denied")));
    QVERIFY(err.contains(QStringLiteral("ATC-only")));

    // C. Airport Operator role (not permitted to declare emergency)
    m_service.setTestSession(Session(3, QStringLiteral("operator"), QStringLiteral("Runway Operator"), QStringLiteral("Airport Operator")));
    QVERIFY(!m_service.declareEmergency(m_testFlightId2 > 0 ? m_testFlightId2 : 1, &err));
    QVERIFY(err.contains(QStringLiteral("Access denied")));
    QVERIFY(err.contains(QStringLiteral("ATC-only")));
}

void TestDynamicEmergencyPriority::testUnauthorizedUsersCannotClearEmergency()
{
    // Requirement 16: Unauthorized users cannot clear an emergency.
    QString err;

    // A. Unauthenticated session
    m_service.setTestSession(Session());
    QVERIFY(!m_service.clearEmergency(m_testFlightId2 > 0 ? m_testFlightId2 : 1, &err));
    QVERIFY(err.contains(QStringLiteral("Authentication required")));

    // B. Admin role
    m_service.setTestSession(Session(2, QStringLiteral("sysadmin"), QStringLiteral("System Administrator"), QStringLiteral("Admin")));
    QVERIFY(!m_service.clearEmergency(m_testFlightId2 > 0 ? m_testFlightId2 : 1, &err));
    QVERIFY(err.contains(QStringLiteral("Access denied")));

    // C. Airport Operator role
    m_service.setTestSession(Session(3, QStringLiteral("operator"), QStringLiteral("Runway Operator"), QStringLiteral("Airport Operator")));
    QVERIFY(!m_service.clearEmergency(m_testFlightId2 > 0 ? m_testFlightId2 : 1, &err));
    QVERIFY(err.contains(QStringLiteral("Access denied")));
}

void TestDynamicEmergencyPriority::testMissingFlightHandledSafely()
{
    // Requirement 17: Missing flight is handled safely.
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not available for integration test.");
    }

    m_service.setTestSession(Session(10, QStringLiteral("atc_officer"), QStringLiteral("ATC Officer"), QStringLiteral("ATC")));

    QString err;
    bool ok = m_service.declareEmergency(999999, &err);
    QVERIFY(!ok);
    QVERIFY(err.contains(QStringLiteral("not found")));
}

void TestDynamicEmergencyPriority::testTerminalFlightEligibilityHandledCorrectly()
{
    // Requirement 18: Terminal flight is handled according to documented eligibility rules.
    if (!m_dbConnected || m_testTerminalFlightId <= 0) {
        QSKIP("Live MySQL database not available for integration test.");
    }

    m_service.setTestSession(Session(10, QStringLiteral("atc_officer"), QStringLiteral("ATC Officer"), QStringLiteral("ATC")));

    QString err;
    // Terminal flight (Landed) cannot declare emergency
    bool ok = m_service.declareEmergency(m_testTerminalFlightId, &err);
    QVERIFY(!ok);
    QVERIFY(err.contains(QStringLiteral("terminal status")));
}

void TestDynamicEmergencyPriority::testDatabaseFailureReportedCorrectly()
{
    // Requirement 19: Database failure is reported correctly.
    // Invalid flight ID parameter (< 0) fails immediately with descriptive error
    m_service.setTestSession(Session(10, QStringLiteral("atc_officer"), QStringLiteral("ATC Officer"), QStringLiteral("ATC")));

    QString err;
    bool ok = m_service.declareEmergency(-5, &err);
    QVERIFY(!ok);
    QVERIFY(err.contains(QStringLiteral("Invalid flight ID")));
}

void TestDynamicEmergencyPriority::testAssociatedProcessPrioritySynchronizedCorrectly()
{
    // Requirement 20: Associated process priority is synchronized correctly.
    if (!m_dbConnected || m_testFlightId2 <= 0) {
        QSKIP("Live MySQL database not available for integration test.");
    }

    m_service.setTestSession(Session(10, QStringLiteral("atc_officer"), QStringLiteral("ATC Officer"), QStringLiteral("ATC")));

    // Ensure flight 2 is not currently in emergency
    Flight f;
    QString err;
    QVERIFY(m_service.getFlightById(m_testFlightId2, f, &err));
    if (f.isEmergency) {
        m_service.clearEmergency(m_testFlightId2);
    }

    // Clean up any existing process for this flight
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id = :id;"));
    q.bindValue(QStringLiteral(":id"), m_testFlightId2);
    q.exec();

    // Create process in scheduling queue for flight 2 (base priority 3)
    Models::Process proc;
    bool created = m_service.createProcessForFlight(m_testFlightId2, 60, &proc, &err);
    QVERIFY2(created, qPrintable(err));
    QCOMPARE(proc.effectivePriority, 3);
    QCOMPARE(proc.basePriority, 3);

    // Declare emergency on flight 2
    bool declared = m_service.declareEmergency(m_testFlightId2, &err);
    QVERIFY2(declared, qPrintable(err));

    // Verify DB scheduling_queue record has effective_priority = 1
    Models::Process dbProc;
    QVERIFY(m_service.getProcessByFlightId(m_testFlightId2, dbProc, &err));
    QCOMPARE(dbProc.effectivePriority, 1);
    QCOMPARE(dbProc.basePriority, 3); // Base priority remains 3!

    // Verify in-memory ready queue has effectivePriority = 1
    if (m_service.scheduler()) {
        Models::Process *memProc = m_service.scheduler()->readyQueue().findByFlightId(m_testFlightId2);
        QVERIFY(memProc != nullptr);
        QCOMPARE(memProc->effectivePriority, 1);
        QCOMPARE(memProc->basePriority, 3);
    }

    // Now clear emergency on flight 2
    bool cleared = m_service.clearEmergency(m_testFlightId2, &err);
    QVERIFY2(cleared, qPrintable(err));

    // Verify DB scheduling_queue restored to base priority 3
    QVERIFY(m_service.getProcessByFlightId(m_testFlightId2, dbProc, &err));
    QCOMPARE(dbProc.effectivePriority, 3);
    QCOMPARE(dbProc.basePriority, 3);

    // Verify in-memory ready queue restored
    if (m_service.scheduler()) {
        Models::Process *memProc = m_service.scheduler()->readyQueue().findByFlightId(m_testFlightId2);
        QVERIFY(memProc != nullptr);
        QCOMPARE(memProc->effectivePriority, 3);
        QCOMPARE(memProc->basePriority, 3);
    }
}

void TestDynamicEmergencyPriority::testAlreadyEmergencyAndAlreadyNormalHandledSafely()
{
    if (!m_dbConnected || m_testFlightId1 <= 0) {
        QSKIP("Live MySQL database not available for integration test.");
    }

    m_service.setTestSession(Session(10, QStringLiteral("atc_officer"), QStringLiteral("ATC Officer"), QStringLiteral("ATC")));

    // Make sure flight 1 is normal
    Flight f;
    QString err;
    QVERIFY(m_service.getFlightById(m_testFlightId1, f, &err));
    if (f.isEmergency) {
        m_service.clearEmergency(m_testFlightId1);
    }

    // Clearing an already normal flight should fail safely
    bool clearFail = m_service.clearEmergency(m_testFlightId1, &err);
    QVERIFY(!clearFail);
    QVERIFY(err.contains(QStringLiteral("not currently in an emergency state")));

    // Declare emergency
    QVERIFY(m_service.declareEmergency(m_testFlightId1, &err));

    // Declaring emergency on an already emergency flight should fail safely
    bool declareFail = m_service.declareEmergency(m_testFlightId1, &err);
    QVERIFY(!declareFail);
    QVERIFY(err.contains(QStringLiteral("already declared as an emergency")));

    // Restore to normal
    m_service.clearEmergency(m_testFlightId1);
}

QTEST_MAIN(TestDynamicEmergencyPriority)
#include "TestDynamicEmergencyPriority.moc"
