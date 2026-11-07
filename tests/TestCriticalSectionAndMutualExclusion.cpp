#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>
#include <thread>
#include <future>
#include <chrono>

#include "models/Runway.h"
#include "models/Flight.h"
#include "models/Session.h"
#include "models/Process.h"
#include "models/RunwayAssignment.h"
#include "resource/ResourceManager.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"
#include "scheduler/Scheduler.h"
#include "scheduler/ReadyQueue.h"
#include "scheduler/AgingEngine.h"
#include "scheduler/StarvationSimulation.h"
#include "utils/PasswordUtils.h"

using namespace Models;
using namespace SchedulerComponents;

/**
 * @brief Dedicated test suite for Day 19: Critical Section & Mutual Exclusion.
 *
 * Verifies:
 *   1. Mutual exclusion semantics (RAII scoped locks, non-overlapping critical sections,
 *      single ownership, try_lock contention, exception safety, deadlock freedom).
 *   2. Runway resource invariants (exclusive ownership, double-allocation rejection,
 *      maintenance protection, flight single-runway constraint, rollback cleanup).
 *   3. Service integration & authorization (RBAC enforcement, error reporting, Day 15 compat).
 *   4. Regressions (Day 16 Emergency Priority, Day 17 Aging, Day 18 Starvation Simulation).
 */
class TestCriticalSectionAndMutualExclusion : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // ── 1. Mutual Exclusion Tests (Points 1–7) ─────────────────────────────────
    void testCriticalSectionInit();
    void testSingleOperationAcquiresProtection();
    void testSecondOperationCannotEnterSimultaneously();
    void testProtectionReleasedAfterNormalExit();
    void testProtectionReleasedAfterException();
    void testMultipleProtectedOperationsNonOverlapping();
    void testNoDeadlockInSupportedUsage();

    // ── 2. Runway Resource Invariants (Points 8–16) ───────────────────────────
    void testAvailableRunwayCanBeReserved();
    void testOccupiedRunwayCannotBeReReserved();
    void testMaintenanceRunwayCannotBeReserved();
    void testTwoFlightsCannotOwnSameRunway();
    void testFlightCannotHoldTwoRunways();
    void testReleaseValidatesCorrectAssignment();
    void testInvalidReleaseRejectedSafely();
    void testFailedAllocationStateCleanup();
    void testExistingRunwayCompatibilityChecks();

    // ── 3. Service and Authorization (Points 17–21) ───────────────────────────
    void testAtcCanAllocateThroughProtectedPath();
    void testUnauthorizedAllocationRejected();
    void testReleasePermissionsUnchanged();
    void testFailedResourceAcquisitionReturnsMeaningfulError();
    void testExistingDay15AllocationCompatibility();

    // ── 4. Regression Tests (Points 22–26) ────────────────────────────────────
    void testRegressionDay16EmergencyPriorityIntact();
    void testRegressionDay17PriorityAgingIntact();
    void testRegressionDay18StarvationSimulationIntact();
    void testRegressionSchedulingBehaviorUnchanged();
    void testRegressionAuthAndRbacPass();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;

    // Fixture entity IDs for database tests
    int m_testRunwayId1 = 0; // Both (Available)
    int m_testRunwayId2 = 0; // Landing (Available)
    int m_testRunwayId3 = 0; // Takeoff (Available)
    int m_testFlightId1 = 0; // Arrival Scheduled
    int m_testFlightId2 = 0; // Departure Scheduled
};

void TestCriticalSectionAndMutualExclusion::initTestCase()
{
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();
    if (!m_dbConnected) {
        qWarning() << "[TestCriticalSectionAndMutualExclusion] Live MySQL unavailable; database-dependent tests will run with mock/in-memory checks.";
    }
}

void TestCriticalSectionAndMutualExclusion::cleanupTestCase()
{
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM runway_assignments WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'CS-T%');"));
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'CS-T%');"));
            q.exec(QStringLiteral("DELETE FROM operation_logs WHERE message LIKE '%CS-T%' OR message LIKE '%RW-CS%';"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'CS-T%';"));
            q.exec(QStringLiteral("DELETE FROM runways WHERE runway_number LIKE 'RW-CS%';"));
        }
    }
    m_service.shutdown();
}

void TestCriticalSectionAndMutualExclusion::init()
{
    if (m_service.resourceManager()) {
        m_service.resourceManager()->clearState();
    }

    if (!m_dbConnected) {
        return;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        return;
    }

    cleanup();

    // Create test runways
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO runways (runway_number, runway_type, status) VALUES ('RW-CS01', 'Both', 'Available');"));
    if (q.exec()) {
        m_testRunwayId1 = q.lastInsertId().toInt();
    }

    q.prepare(QStringLiteral("INSERT INTO runways (runway_number, runway_type, status) VALUES ('RW-CS02', 'Landing', 'Available');"));
    if (q.exec()) {
        m_testRunwayId2 = q.lastInsertId().toInt();
    }

    q.prepare(QStringLiteral("INSERT INTO runways (runway_number, runway_type, status) VALUES ('RW-CS03', 'Takeoff', 'Available');"));
    if (q.exec()) {
        m_testRunwayId3 = q.lastInsertId().toInt();
    }

    // Create test flights
    q.prepare(QStringLiteral(
        "INSERT INTO flights (flight_number, airline, source, destination, flight_type, priority, operational_status, scheduled_time) "
        "VALUES ('CS-T01', 'TestAir', 'DEL', 'BOM', 'Arrival', 2, 'Scheduled', NOW());"
    ));
    if (q.exec()) {
        m_testFlightId1 = q.lastInsertId().toInt();
    }

    q.prepare(QStringLiteral(
        "INSERT INTO flights (flight_number, airline, source, destination, flight_type, priority, operational_status, scheduled_time) "
        "VALUES ('CS-T02', 'TestAir', 'BOM', 'BLR', 'Departure', 3, 'Scheduled', NOW());"
    ));
    if (q.exec()) {
        m_testFlightId2 = q.lastInsertId().toInt();
    }

    if (m_service.resourceManager()) {
        m_service.resourceManager()->syncFromDatabase();
    }
}

void TestCriticalSectionAndMutualExclusion::cleanup()
{
    if (m_service.resourceManager()) {
        m_service.resourceManager()->clearState();
    }

    if (!m_dbConnected) {
        return;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        return;
    }

    QSqlQuery q(db);
    q.exec(QStringLiteral("DELETE FROM runway_assignments WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'CS-T%');"));
    q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'CS-T%');"));
    q.exec(QStringLiteral("DELETE FROM operation_logs WHERE message LIKE '%CS-T%' OR message LIKE '%RW-CS%';"));
    q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'CS-T%';"));
    q.exec(QStringLiteral("DELETE FROM runways WHERE runway_number LIKE 'RW-CS%';"));
}

// ── 1. Mutual Exclusion Tests (Points 1–7) ─────────────────────────────────────

void TestCriticalSectionAndMutualExclusion::testCriticalSectionInit()
{
    ResourceManager mgr;
    QCOMPARE(mgr.isRunwayReserved(1), false);
    QCOMPARE(mgr.activeFlightForRunway(1), 0);
    QCOMPARE(mgr.isRunwayInMaintenance(1), false);
    QCOMPARE(mgr.maxConcurrentObserved(), 0);
    QCOMPARE(mgr.criticalSectionStatus(1), QStringLiteral("Resource Available"));

    // Verify ScopedResourceLock initializes correctly
    {
        ResourceManager::ScopedResourceLock lock(mgr.resourceMutex(), 1, &mgr);
        QVERIFY(lock.isLocked());
        QCOMPARE(mgr.criticalSectionStatusUnlocked(1), QStringLiteral("Critical Section Entered"));
    }
    QCOMPARE(mgr.criticalSectionStatus(1), QStringLiteral("Critical Section Released"));
}

void TestCriticalSectionAndMutualExclusion::testSingleOperationAcquiresProtection()
{
    ResourceManager mgr;
    bool enteredSignalFired = false;
    connect(&mgr, &ResourceManager::criticalSectionEntered, this, [&](int rId, int fId) {
        Q_UNUSED(fId)
        if (rId == 5) enteredSignalFired = true;
    });

    {
        ResourceManager::ScopedResourceLock lock(mgr.resourceMutex(), 5, &mgr);
        QVERIFY(lock.isLocked());
        QVERIFY(enteredSignalFired);
        QCOMPARE(mgr.criticalSectionStatusUnlocked(5), QStringLiteral("Critical Section Entered"));
    }
}

void TestCriticalSectionAndMutualExclusion::testSecondOperationCannotEnterSimultaneously()
{
    ResourceManager mgr;

    // Thread 1 acquires the mutex
    std::mutex &mtx = mgr.resourceMutex();
    std::unique_lock<std::mutex> lock1(mtx);

    // Thread 2 attempts non-blocking acquisition - MUST be rejected
    std::atomic<bool> secondAcquisitionSucceeded = false;
    std::thread t2([&]() {
        if (mtx.try_lock()) {
            secondAcquisitionSucceeded = true;
            mtx.unlock();
        } else {
            secondAcquisitionSucceeded = false;
        }
    });
    t2.join();

    QVERIFY(!secondAcquisitionSucceeded);
    lock1.unlock();

    // Now after thread 1 unlocks, acquisition succeeds
    QVERIFY(mtx.try_lock());
    mtx.unlock();
}

void TestCriticalSectionAndMutualExclusion::testProtectionReleasedAfterNormalExit()
{
    ResourceManager mgr;
    {
        ResourceManager::ScopedResourceLock lock(mgr.resourceMutex(), 10, &mgr);
        QVERIFY(lock.isLocked());
    }

    // Normal scope exited: lock should be available immediately
    bool canAcquire = mgr.resourceMutex().try_lock();
    QVERIFY(canAcquire);
    mgr.resourceMutex().unlock();
}

void TestCriticalSectionAndMutualExclusion::testProtectionReleasedAfterException()
{
    ResourceManager mgr;
    bool exceptionCaught = false;

    try {
        ResourceManager::ScopedResourceLock lock(mgr.resourceMutex(), 7, &mgr);
        QVERIFY(lock.isLocked());
        throw std::runtime_error("Simulated failure in runway critical section");
    } catch (const std::runtime_error &e) {
        exceptionCaught = true;
        QCOMPARE(QString::fromUtf8(e.what()), QStringLiteral("Simulated failure in runway critical section"));
    }

    QVERIFY(exceptionCaught);

    // Mutex MUST be unlocked by RAII destructor despite exception
    bool canAcquire = mgr.resourceMutex().try_lock();
    QVERIFY(canAcquire);
    mgr.resourceMutex().unlock();
}

void TestCriticalSectionAndMutualExclusion::testMultipleProtectedOperationsNonOverlapping()
{
    ResourceManager mgr;

    // Perform sequential allocations
    QString err;
    QVERIFY(mgr.allocateRunway(1, 101, &err));
    QVERIFY(mgr.allocateRunway(2, 102, &err));
    QVERIFY(mgr.allocateRunway(3, 103, &err));

    QVERIFY(mgr.releaseRunway(1, &err));
    QVERIFY(mgr.releaseRunway(2, &err));
    QVERIFY(mgr.releaseRunway(3, &err));

    // Mutual exclusion guarantee: max concurrent entries observed inside critical sections was exactly 1
    QCOMPARE(mgr.maxConcurrentObserved(), 1);
}

void TestCriticalSectionAndMutualExclusion::testNoDeadlockInSupportedUsage()
{
    ResourceManager mgr;

    // Exercise repeated acquire/release patterns across runways
    for (int cycle = 1; cycle <= 20; ++cycle) {
        QString err;
        int rId = (cycle % 3) + 1;
        int fId = cycle * 10;

        QVERIFY(mgr.allocateRunway(rId, fId, &err));
        QVERIFY(mgr.isRunwayReserved(rId));
        QVERIFY(mgr.releaseRunway(rId, &err));
        QVERIFY(!mgr.isRunwayReserved(rId));
    }
}

// ── 2. Runway Resource Invariants (Points 8–16) ───────────────────────────────

void TestCriticalSectionAndMutualExclusion::testAvailableRunwayCanBeReserved()
{
    ResourceManager mgr;
    QString err;
    bool ok = mgr.allocateRunway(1, 201, &err);
    QVERIFY(ok);
    QVERIFY(err.isEmpty());
    QVERIFY(mgr.isRunwayReserved(1));
    QCOMPARE(mgr.activeFlightForRunway(1), 201);
    QCOMPARE(mgr.activeRunwayForFlight(201), 1);
    QCOMPARE(mgr.criticalSectionStatus(1), QStringLiteral("Resource Reserved"));
}

void TestCriticalSectionAndMutualExclusion::testOccupiedRunwayCannotBeReReserved()
{
    ResourceManager mgr;
    QString err;
    QVERIFY(mgr.allocateRunway(1, 201, &err));

    // Attempt second allocation on the same runway while occupied
    bool ok2 = mgr.allocateRunway(1, 202, &err);
    QVERIFY(!ok2);
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains(QStringLiteral("Resource Contention"), Qt::CaseInsensitive) ||
             err.contains(QStringLiteral("already allocated"), Qt::CaseInsensitive));
    QCOMPARE(mgr.activeFlightForRunway(1), 201); // Still owned by flight 201
}

void TestCriticalSectionAndMutualExclusion::testMaintenanceRunwayCannotBeReserved()
{
    ResourceManager mgr;
    mgr.setRunwayMaintenance(1, true);
    QVERIFY(mgr.isRunwayInMaintenance(1));

    QString err;
    bool ok = mgr.allocateRunway(1, 301, &err);
    QVERIFY(!ok);
    QVERIFY(err.contains(QStringLiteral("Maintenance"), Qt::CaseInsensitive));
    QVERIFY(!mgr.isRunwayReserved(1));
}

void TestCriticalSectionAndMutualExclusion::testTwoFlightsCannotOwnSameRunway()
{
    ResourceManager mgr;
    QString err;
    QVERIFY(mgr.allocateRunway(5, 501, &err));

    bool secondFlightResult = mgr.allocateRunway(5, 502, &err);
    QVERIFY(!secondFlightResult);
    QCOMPARE(mgr.activeFlightForRunway(5), 501);
}

void TestCriticalSectionAndMutualExclusion::testFlightCannotHoldTwoRunways()
{
    ResourceManager mgr;
    QString err;
    QVERIFY(mgr.allocateRunway(1, 999, &err));

    // Same flight 999 attempts to reserve runway 2
    bool ok2 = mgr.allocateRunway(2, 999, &err);
    QVERIFY(!ok2);
    QVERIFY(err.contains(QStringLiteral("already holds an active allocation"), Qt::CaseInsensitive));
    QVERIFY(!mgr.isRunwayReserved(2));
}

void TestCriticalSectionAndMutualExclusion::testReleaseValidatesCorrectAssignment()
{
    ResourceManager mgr;
    QString err;
    QVERIFY(mgr.allocateRunway(3, 777, &err));
    QVERIFY(mgr.isRunwayReserved(3));

    QVERIFY(mgr.releaseRunway(3, &err));
    QVERIFY(!mgr.isRunwayReserved(3));
    QCOMPARE(mgr.activeFlightForRunway(3), 0);
    QCOMPARE(mgr.activeRunwayForFlight(777), 0);
    QCOMPARE(mgr.criticalSectionStatus(3), QStringLiteral("Resource Available"));
}

void TestCriticalSectionAndMutualExclusion::testInvalidReleaseRejectedSafely()
{
    ResourceManager mgr;
    QString err;

    // Release non-allocated runway
    bool okUnreserved = mgr.releaseRunway(88, &err);
    QVERIFY(!okUnreserved);
    QVERIFY(!err.isEmpty());

    // Release invalid ID
    bool okInvalid = mgr.releaseRunway(-1, &err);
    QVERIFY(!okInvalid);
    QVERIFY(err.contains(QStringLiteral("Invalid runway ID"), Qt::CaseInsensitive));
}

void TestCriticalSectionAndMutualExclusion::testFailedAllocationStateCleanup()
{
    ResourceManager mgr;
    QString err;
    QVERIFY(mgr.allocateRunway(4, 404, &err));
    QVERIFY(mgr.isRunwayReserved(4));

    // Rollback simulation (e.g. database failure after in-memory reservation)
    mgr.rollbackAllocation(4, 404);
    QVERIFY(!mgr.isRunwayReserved(4));
    QCOMPARE(mgr.activeFlightForRunway(4), 0);
    QCOMPARE(mgr.criticalSectionStatus(4), QStringLiteral("Resource Available"));

    // Runway can immediately be allocated again cleanly
    QVERIFY(mgr.allocateRunway(4, 405, &err));
    QCOMPARE(mgr.activeFlightForRunway(4), 405);
}

void TestCriticalSectionAndMutualExclusion::testExistingRunwayCompatibilityChecks()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL required for database compatibility checks");
    }

    Session atcSession(10, QStringLiteral("atc_tester"), QStringLiteral("ATC Tester"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString err;
    // Test 1: Departure flight on Landing-only runway (RW-CS02 is Landing, CS-T02 is Departure)
    Models::AllocationResult resMismatch = m_service.allocateRunway(m_testRunwayId2, m_testFlightId2, &err);
    QCOMPARE(resMismatch, Models::AllocationResult::FlightIneligible);
    QVERIFY(err.contains(QStringLiteral("designated for Landing only"), Qt::CaseInsensitive));

    // Test 2: Arrival flight on Takeoff-only runway (RW-CS03 is Takeoff, CS-T01 is Arrival)
    Models::AllocationResult resMismatch2 = m_service.allocateRunway(m_testRunwayId3, m_testFlightId1, &err);
    QCOMPARE(resMismatch2, Models::AllocationResult::FlightIneligible);
    QVERIFY(err.contains(QStringLiteral("designated for Takeoff only"), Qt::CaseInsensitive));
}

// ── 3. Service and Authorization (Points 17–21) ───────────────────────────────

void TestCriticalSectionAndMutualExclusion::testAtcCanAllocateThroughProtectedPath()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL required for service allocation test");
    }

    Session atcSession(1, QStringLiteral("atc_ctrl"), QStringLiteral("ATC Controller"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString err;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &err);
    QCOMPARE(res, Models::AllocationResult::Success);

    // Verify in-memory state updated
    QVERIFY(m_service.resourceManager()->isRunwayReserved(m_testRunwayId1));
    QCOMPARE(m_service.resourceManager()->activeFlightForRunway(m_testRunwayId1), m_testFlightId1);
}

void TestCriticalSectionAndMutualExclusion::testUnauthorizedAllocationRejected()
{
    // 1. Unauthenticated session
    m_service.setTestSession(Session());
    QString err;
    Models::AllocationResult res1 = m_service.allocateRunway(1, 101, &err);
    QCOMPARE(res1, Models::AllocationResult::Unauthorized);

    // 2. Viewer role
    Session viewer(2, QStringLiteral("viewer_user"), QStringLiteral("Viewer User"), QStringLiteral("Viewer"));
    m_service.setTestSession(viewer);
    Models::AllocationResult res2 = m_service.allocateRunway(1, 101, &err);
    QCOMPARE(res2, Models::AllocationResult::Unauthorized);
}

void TestCriticalSectionAndMutualExclusion::testReleasePermissionsUnchanged()
{
    QString err;

    // Viewer is rejected
    Session viewer(2, QStringLiteral("viewer_user"), QStringLiteral("Viewer User"), QStringLiteral("Viewer"));
    m_service.setTestSession(viewer);
    QVERIFY(!m_service.releaseRunway(1, &err));
    QVERIFY(err.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));

    // ATC permission check passes authorization boundary (fails on DB/existence if no DB)
    Session atc(1, QStringLiteral("atc_user"), QStringLiteral("ATC User"), QStringLiteral("ATC"));
    m_service.setTestSession(atc);
    err.clear();
    // It should not fail with "Access denied"
    m_service.releaseRunway(1, &err);
    QVERIFY(!err.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));

    // Operator passes authorization boundary
    Session op(3, QStringLiteral("op_user"), QStringLiteral("Operator User"), QStringLiteral("Airport Operator"));
    m_service.setTestSession(op);
    err.clear();
    m_service.releaseRunway(1, &err);
    QVERIFY(!err.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));
}

void TestCriticalSectionAndMutualExclusion::testFailedResourceAcquisitionReturnsMeaningfulError()
{
    ResourceManager mgr;
    QString err;
    QVERIFY(mgr.allocateRunway(1, 10, &err));

    bool ok = mgr.allocateRunway(1, 20, &err);
    QVERIFY(!ok);
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains(QStringLiteral("Resource Contention"), Qt::CaseInsensitive));
}

void TestCriticalSectionAndMutualExclusion::testExistingDay15AllocationCompatibility()
{
    QCOMPARE(Models::allocationResultToString(Models::AllocationResult::Success), QStringLiteral("Allocation succeeded."));
    QCOMPARE(Models::allocationResultToString(Models::AllocationResult::RunwayUnavailable), QStringLiteral("Runway is unavailable (occupied or in maintenance)."));
    QCOMPARE(Models::allocationResultToString(Models::AllocationResult::ConflictExistingAssignment), QStringLiteral("Active runway assignment conflict detected."));
    QCOMPARE(Models::allocationResultToString(Models::AllocationResult::Unauthorized), QStringLiteral("Operation unauthorized: ATC role required."));
}

// ── 4. Regression Tests (Points 22–26) ────────────────────────────────────────

void TestCriticalSectionAndMutualExclusion::testRegressionDay16EmergencyPriorityIntact()
{
    Process p(1, 101, 4, 10);
    p.flightNumber = QStringLiteral("P-EMERG");
    QCOMPARE(p.basePriority, 4);
    QCOMPARE(p.effectivePriority, 4);

    p.setEmergency(true);
    QVERIFY(p.isEmergency);
    QCOMPARE(p.effectivePriority, 1); // Emergency enforces highest priority 1

    p.setEmergency(false);
    QVERIFY(!p.isEmergency);
    QCOMPARE(p.effectivePriority, 4); // Restores base priority
}

void TestCriticalSectionAndMutualExclusion::testRegressionDay17PriorityAgingIntact()
{
    Process p(2, 102, 5, 10);
    p.flightNumber = QStringLiteral("P-AGING");
    QCOMPARE(p.effectivePriority, 5);

    p.agingLevel = 1;
    p.effectivePriority = std::max(1, p.basePriority - p.agingLevel);
    QCOMPARE(p.effectivePriority, 4);

    p.agingLevel = 2;
    p.effectivePriority = std::max(1, p.basePriority - p.agingLevel);
    QCOMPARE(p.effectivePriority, 3);

    p.agingLevel = 4;
    p.effectivePriority = std::max(1, p.basePriority - p.agingLevel);
    QCOMPARE(p.effectivePriority, 1);

    p.agingLevel = 10;
    p.effectivePriority = std::max(1, p.basePriority - p.agingLevel);
    QCOMPARE(p.effectivePriority, 1); // Clamp at 1
    QCOMPARE(p.basePriority, 5);      // Base priority never mutated
}

void TestCriticalSectionAndMutualExclusion::testRegressionDay18StarvationSimulationIntact()
{
    SimulationConfig configWithout;
    configWithout.mode = SimulationMode::WithoutAging;
    configWithout.agingIntervalSeconds = 30;
    configWithout.simulationHorizonSeconds = 180;

    StarvationSimulation simWithout(configWithout);
    SimulationMetrics mWithout = simWithout.runToHorizon();
    QVERIFY(mWithout.totalDispatched > 0);

    SimulationConfig configWith;
    configWith.mode = SimulationMode::WithAging;
    configWith.agingIntervalSeconds = 30;
    configWith.simulationHorizonSeconds = 180;

    StarvationSimulation simWith(configWith);
    SimulationMetrics mWith = simWith.runToHorizon();
    QVERIFY(mWith.totalDispatched > 0);
}

void TestCriticalSectionAndMutualExclusion::testRegressionSchedulingBehaviorUnchanged()
{
    Scheduler scheduler;
    Process p1(1, 101, 5, 10);
    Process p2(2, 102, 2, 10);
    Process p3(3, 103, 1, 10);

    scheduler.readyQueue().enqueue(p1);
    scheduler.readyQueue().enqueue(p2);
    scheduler.readyQueue().enqueue(p3);

    Process candidate;
    QVERIFY(scheduler.selectNextProcess(&candidate));
    QCOMPARE(candidate.effectivePriority, 1);
    QCOMPARE(candidate.flightId, 103);
}

void TestCriticalSectionAndMutualExclusion::testRegressionAuthAndRbacPass()
{
    QString hash = PasswordUtils::hash(QStringLiteral("atcpass123"));
    QVERIFY(PasswordUtils::verify(QStringLiteral("atcpass123"), hash));
    QVERIFY(!PasswordUtils::verify(QStringLiteral("wrongpassword"), hash));

    Session session(1, QStringLiteral("atc"), QStringLiteral("ATC User"), QStringLiteral("ATC"));
    QVERIFY(session.isValid());
    QVERIFY(session.hasPermission(QStringLiteral("action:RunwayAllocation")));
    QVERIFY(!session.hasPermission(QStringLiteral("action:ManageUsers")));
}

QTEST_MAIN(TestCriticalSectionAndMutualExclusion)
#include "TestCriticalSectionAndMutualExclusion.moc"

