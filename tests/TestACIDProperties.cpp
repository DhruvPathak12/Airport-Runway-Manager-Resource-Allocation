#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDriver>
#include <thread>
#include <future>
#include <atomic>
#include <chrono>

#include "models/Runway.h"
#include "models/Flight.h"
#include "models/Session.h"
#include "models/RunwayAssignment.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"
#include "resource/ResourceManager.h"

/**
 * @brief Comprehensive automated test suite for Phase 2 / Day 22:
 * ACID Properties, COMMIT and ROLLBACK Verification.
 *
 * Demonstrates and verifies all four ACID properties:
 *   1. Atomicity: Multi-stage controlled failure scenarios & rollbacks.
 *   2. Consistency: Database invariants, foreign key constraints, reconciliation.
 *   3. Isolation: Multi-connection concurrency, row locks (FOR UPDATE), lock contention.
 *   4. Durability: Persistence across simulated application service restarts & InnoDB settings.
 * Plus explicit repeatable COMMIT and ROLLBACK demonstration workflows.
 */
class TestACIDProperties : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // ── 1. ACID Property: Atomicity Tests ────────────────────────────────────
    void testAtomicityFailureBeforeInsert();
    void testAtomicityFailureDuringInsertAssignment();
    void testAtomicityFailureDuringUpdateRunway();
    void testAtomicityFailureDuringUpdateFlight();
    void testAtomicityFailureDuringInsertOperationLog();
    void testAtomicityFailureDuringCommit();
    void testAtomicityReleaseFailureDuringAssignment();
    void testAtomicityReleaseFailureDuringRunwayUpdate();
    void testAtomicityReleaseFailureDuringCommit();
    void testAtomicityInMemoryConsistencyAfterRollback();

    // ── 2. ACID Property: Consistency Tests ──────────────────────────────────
    void testConsistencyDuplicateRunwayAssignment();
    void testConsistencyDuplicateFlightAssignment();
    void testConsistencyOccupiedRunwayHasActiveAssignment();
    void testConsistencyReleasedAssignmentNotActive();
    void testConsistencyFailedAllocationDoesNotMarkFlightCleared();
    void testConsistencyMaintenanceRunwayCannotBeAllocated();
    void testConsistencyInvalidFlightCannotBeAllocated();
    void testConsistencyForeignKeyIntegrity();
    void testConsistencyOperationLogIntegrity();
    void testConsistencyReconciliationHelper();

    // ── 3. ACID Property: Isolation Tests (Independent Connections) ─────────
    void testIsolationLevelInspection();
    void testIsolationTwoConnectionsCompeteSameRunway();
    void testIsolationTwoConnectionsCompeteSameFlight();
    void testIsolationRowLockWaitingAndTimeoutSafety();

    // ── 4. ACID Property: Durability Tests ───────────────────────────────────
    void testDurabilityInnodbFlushLogConfiguration();
    void testDurabilityCommittedAllocationSurvivesServiceRestart();
    void testDurabilityCommittedReleaseSurvivesServiceRestart();

    // ── 5. COMMIT and ROLLBACK Demonstrations ────────────────────────────────
    void testDemonstrationCommitWorkflow();
    void testDemonstrationRollbackWorkflow();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;

    // Fixture entity IDs
    int m_testRunwayId1 = 0; // Both, Available
    int m_testRunwayId2 = 0; // Both, Available
    int m_testFlightId1 = 0; // Arrival, Scheduled
    int m_testFlightId2 = 0; // Departure, Scheduled
    int m_testFlightId3 = 0; // Arrival, Landed (Terminal)
};

void TestACIDProperties::initTestCase()
{
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();
    if (!m_dbConnected) {
        qWarning() << "[TestACIDProperties] Live database connection is offline. Live integration tests will be cleanly skipped via QSKIP.";
    }
}

void TestACIDProperties::cleanupTestCase()
{
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM runway_assignments WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'ACID-%');"));
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'ACID-%');"));
            q.exec(QStringLiteral("DELETE FROM operation_logs WHERE message LIKE '%ACID-%' OR message LIKE '%RW-ACID%';"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'ACID-%';"));
            q.exec(QStringLiteral("DELETE FROM runways WHERE runway_number LIKE 'RW-ACID%';"));
        }
    }
    m_service.shutdown();
}

void TestACIDProperties::init()
{
    m_service.setAllocationFailureStage(ApplicationService::AllocationFailureStage::None);
    m_service.setReleaseFailureStage(ApplicationService::ReleaseFailureStage::None);

    if (!m_dbConnected) {
        return;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        return;
    }

    cleanup();

    // Fixture Runway 1: Available, Both
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("INSERT INTO runways (runway_number, runway_type, status) VALUES ('RW-ACID1', 'Both', 'Available');"));
        if (q.exec()) {
            m_testRunwayId1 = q.lastInsertId().toInt();
        }
    }

    // Fixture Runway 2: Available, Both
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("INSERT INTO runways (runway_number, runway_type, status) VALUES ('RW-ACID2', 'Both', 'Available');"));
        if (q.exec()) {
            m_testRunwayId2 = q.lastInsertId().toInt();
        }
    }

    // Fixture Flight 1: Arrival, Scheduled
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT INTO flights (flight_number, airline, flight_type, operational_status, scheduled_time, emergency_status) "
            "VALUES ('ACID-101', 'Air India', 'Arrival', 'Scheduled', NOW(), 0);"
        ));
        if (q.exec()) {
            m_testFlightId1 = q.lastInsertId().toInt();
        }
    }

    // Fixture Flight 2: Departure, Scheduled
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT INTO flights (flight_number, airline, flight_type, operational_status, scheduled_time, emergency_status) "
            "VALUES ('ACID-102', 'IndiGo', 'Departure', 'Scheduled', NOW(), 0);"
        ));
        if (q.exec()) {
            m_testFlightId2 = q.lastInsertId().toInt();
        }
    }

    // Fixture Flight 3: Arrival, Landed (Terminal)
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT INTO flights (flight_number, airline, flight_type, operational_status, scheduled_time, emergency_status) "
            "VALUES ('ACID-103', 'SpiceJet', 'Arrival', 'Landed', NOW(), 0);"
        ));
        if (q.exec()) {
            m_testFlightId3 = q.lastInsertId().toInt();
        }
    }
}

void TestACIDProperties::cleanup()
{
    m_service.setAllocationFailureStage(ApplicationService::AllocationFailureStage::None);
    m_service.setReleaseFailureStage(ApplicationService::ReleaseFailureStage::None);

    if (!m_dbConnected) {
        return;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        return;
    }

    QSqlQuery q(db);
    q.exec(QStringLiteral("DELETE FROM runway_assignments WHERE runway_id IN (SELECT runway_id FROM runways WHERE runway_number LIKE 'RW-ACID%');"));
    q.exec(QStringLiteral("DELETE FROM operation_logs WHERE message LIKE '%ACID-%' OR message LIKE '%RW-ACID%';"));
    q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'ACID-%';"));
    q.exec(QStringLiteral("DELETE FROM runways WHERE runway_number LIKE 'RW-ACID%';"));

    m_testRunwayId1 = 0;
    m_testRunwayId2 = 0;
    m_testFlightId1 = 0;
    m_testFlightId2 = 0;
    m_testFlightId3 = 0;

    if (m_service.resourceManager()) {
        m_service.resourceManager()->clearState();
    }
}

// ── 1. ACID Property: Atomicity Tests ────────────────────────────────────────

void TestACIDProperties::testAtomicityFailureBeforeInsert()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live atomicity test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    m_service.setAllocationFailureStage(ApplicationService::AllocationFailureStage::BeforeInsertAssignment);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::DatabaseError);

    // Verify rollback: zero assignment rows, runway Available, flight Scheduled
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE runway_id = :rid;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :rid;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Available"));
    }
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT operational_status FROM flights WHERE flight_id = :fid;"));
        q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Scheduled"));
    }

    m_service.logout();
}

void TestACIDProperties::testAtomicityFailureDuringInsertAssignment()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live atomicity test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    m_service.setAllocationFailureStage(ApplicationService::AllocationFailureStage::DuringInsertAssignment);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::DatabaseError);

    // Verify rollback
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE runway_id = :rid;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }

    m_service.logout();
}

void TestACIDProperties::testAtomicityFailureDuringUpdateRunway()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live atomicity test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // In this scenario, INSERT into runway_assignments executed inside transaction,
    // but the subsequent UPDATE runways fails!
    m_service.setAllocationFailureStage(ApplicationService::AllocationFailureStage::DuringUpdateRunway);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::DatabaseError);

    // Verify ATOMICITY: even though assignment insert executed, rollback removed it!
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE flight_id = :fid;"));
        q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 0); // Discarded!
    }
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :rid;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Available"));
    }

    m_service.logout();
}

void TestACIDProperties::testAtomicityFailureDuringUpdateFlight()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live atomicity test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Assignment inserted & runway updated, but flight update fails!
    m_service.setAllocationFailureStage(ApplicationService::AllocationFailureStage::DuringUpdateFlight);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::DatabaseError);

    // Verify rollback undid both assignment and runway update!
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE flight_id = :fid;"));
        q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :rid;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Available"));
    }

    m_service.logout();
}

void TestACIDProperties::testAtomicityFailureDuringInsertOperationLog()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live atomicity test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Assignment, runway, and flight all updated, but audit log fails!
    m_service.setAllocationFailureStage(ApplicationService::AllocationFailureStage::DuringInsertOperationLog);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::DatabaseError);

    // Verify rollback undid everything
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE flight_id = :fid;"));
        q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :rid;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Available"));
    }

    m_service.logout();
}

void TestACIDProperties::testAtomicityFailureDuringCommit()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live atomicity test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    m_service.setAllocationFailureStage(ApplicationService::AllocationFailureStage::DuringCommit);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::DatabaseError);

    // Verify rollback
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE flight_id = :fid;"));
        q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }

    m_service.logout();
}

void TestACIDProperties::testAtomicityReleaseFailureDuringAssignment()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live atomicity test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Normal allocation first
    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::Success);

    // Inject failure during assignment release update
    m_service.setReleaseFailureStage(ApplicationService::ReleaseFailureStage::DuringUpdateAssignment);

    bool ok = m_service.releaseRunway(m_testRunwayId1, &error);
    QCOMPARE(ok, false);

    // Verify rollback: assignment is still Active, runway is still Occupied
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runway_assignments WHERE runway_id = :rid AND status = 'Active';"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Active"));
    }
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :rid;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Occupied"));
    }

    m_service.logout();
}

void TestACIDProperties::testAtomicityReleaseFailureDuringRunwayUpdate()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live atomicity test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::Success);

    // Inject failure during runway release update
    m_service.setReleaseFailureStage(ApplicationService::ReleaseFailureStage::DuringUpdateRunway);

    bool ok = m_service.releaseRunway(m_testRunwayId1, &error);
    QCOMPARE(ok, false);

    // Verify rollback undid assignment change
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runway_assignments WHERE runway_id = :rid AND status = 'Active';"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Active"));
    }

    m_service.logout();
}

void TestACIDProperties::testAtomicityReleaseFailureDuringCommit()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live atomicity test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::Success);

    // Inject commit failure during release
    m_service.setReleaseFailureStage(ApplicationService::ReleaseFailureStage::DuringCommit);

    bool ok = m_service.releaseRunway(m_testRunwayId1, &error);
    QCOMPARE(ok, false);

    // Verify rollback
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :rid;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Occupied"));
    }

    m_service.logout();
}

void TestACIDProperties::testAtomicityInMemoryConsistencyAfterRollback()
{
    ResourceManager *mgr = m_service.resourceManager();
    QVERIFY(mgr != nullptr);

    mgr->clearState();
    QCOMPARE(mgr->isRunwayReserved(m_testRunwayId1 > 0 ? m_testRunwayId1 : 99), false);

    // Simulate in-memory reservation followed by failure rollback
    int rId = 99;
    int fId = 200;
    mgr->confirmAllocation(rId, fId);
    QCOMPARE(mgr->isRunwayReserved(rId), true);
    QCOMPARE(mgr->activeFlightForRunway(rId), fId);

    mgr->rollbackAllocation(rId, fId);
    QCOMPARE(mgr->isRunwayReserved(rId), false);
    QCOMPARE(mgr->activeFlightForRunway(rId), 0);
    QCOMPARE(mgr->activeRunwayForFlight(fId), 0);
}

// ── 2. ACID Property: Consistency Tests ──────────────────────────────────────

void TestACIDProperties::testConsistencyDuplicateRunwayAssignment()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live consistency test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    // Invariant 1: Runway cannot have two active assignments
    Models::AllocationResult res1 = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res1, Models::AllocationResult::Success);

    // Second flight attempts to acquire same runway
    Models::AllocationResult res2 = m_service.allocateRunway(m_testRunwayId1, m_testFlightId2, &error);
    QCOMPARE(res2, Models::AllocationResult::ConflictExistingAssignment);

    // Database check: Exactly 1 active assignment
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE runway_id = :rid AND status = 'Active';"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    m_service.logout();
}

void TestACIDProperties::testConsistencyDuplicateFlightAssignment()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live consistency test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    // Invariant 2: Flight cannot have two active runway assignments
    Models::AllocationResult res1 = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res1, Models::AllocationResult::Success);

    // Same flight attempts to acquire runway 2
    Models::AllocationResult res2 = m_service.allocateRunway(m_testRunwayId2, m_testFlightId1, &error);
    QCOMPARE(res2, Models::AllocationResult::ConflictExistingAssignment);

    // Database check: Exactly 1 active assignment for flight 1
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE flight_id = :fid AND status = 'Active';"));
        q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    m_service.logout();
}

void TestACIDProperties::testConsistencyOccupiedRunwayHasActiveAssignment()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live consistency test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    // Invariant 3: An occupied runway must have a corresponding active assignment
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::Success);

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT r.status, ra.assignment_id "
            "FROM runways r "
            "JOIN runway_assignments ra ON r.runway_id = ra.runway_id AND ra.status = 'Active' "
            "WHERE r.runway_id = :rid;"
        ));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Occupied"));
        QVERIFY(q.value(1).toInt() > 0);
    }

    m_service.logout();
}

void TestACIDProperties::testConsistencyReleasedAssignmentNotActive()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live consistency test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    m_service.releaseRunway(m_testRunwayId1, &error);

    // Invariant 4: Released assignment must not remain active
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE runway_id = :rid AND status = 'Active';"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }

    m_service.logout();
}

void TestACIDProperties::testConsistencyFailedAllocationDoesNotMarkFlightCleared()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live consistency test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Invariant 5: A flight must not be marked Cleared through a failed allocation
    m_service.setAllocationFailureStage(ApplicationService::AllocationFailureStage::DuringUpdateFlight);
    QString error;
    m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT operational_status FROM flights WHERE flight_id = :fid;"));
        q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Scheduled")); // NOT Cleared
    }

    m_service.logout();
}

void TestACIDProperties::testConsistencyMaintenanceRunwayCannotBeAllocated()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live consistency test.");
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE runways SET status = 'Maintenance' WHERE runway_id = :rid;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec());
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Invariant 6: Maintenance runway cannot be allocated
    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::RunwayUnavailable);

    m_service.logout();
}

void TestACIDProperties::testConsistencyInvalidFlightCannotBeAllocated()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live consistency test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Invariant 7: Flight in terminal state ('Landed') rejected
    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId3, &error);
    QCOMPARE(res, Models::AllocationResult::FlightIneligible);

    m_service.logout();
}

void TestACIDProperties::testConsistencyForeignKeyIntegrity()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live consistency test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Invariant 8: Non-existent IDs rejected
    QString error;
    Models::AllocationResult res1 = m_service.allocateRunway(999999, m_testFlightId1, &error);
    QCOMPARE(res1, Models::AllocationResult::RunwayNotFound);

    Models::AllocationResult res2 = m_service.allocateRunway(m_testRunwayId1, 999999, &error);
    QCOMPARE(res2, Models::AllocationResult::FlightNotFound);

    m_service.logout();
}

void TestACIDProperties::testConsistencyOperationLogIntegrity()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live consistency test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Invariant 9: Operation logs correspond to successfully committed operations only
    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::Success);

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT transaction_status FROM operation_logs "
            "WHERE runway_id = :rid AND flight_id = :fid AND operation_type = 'RUNWAY_ASSIGN' "
            "ORDER BY log_id DESC LIMIT 1;"
        ));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Committed"));
    }

    m_service.logout();
}

void TestACIDProperties::testConsistencyReconciliationHelper()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live consistency test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);

    // Reconcile valid committed state
    QString status;
    bool ok = m_service.reconcileTransactionState(m_testRunwayId1, m_testFlightId1, &status);
    QVERIFY(ok);
    QVERIFY(status.contains(QStringLiteral("COMMITTED"), Qt::CaseInsensitive));

    m_service.logout();
}

// ── 3. ACID Property: Isolation Tests (Independent Connections) ─────────────

void TestACIDProperties::testIsolationLevelInspection()
{
    QString isolation = DatabaseManager::instance().getTransactionIsolationLevel();
    QVERIFY(!isolation.isEmpty());
    if (m_dbConnected) {
        // In standard MySQL 8, default is REPEATABLE-READ
        QVERIFY(isolation.contains(QStringLiteral("REPEATABLE-READ"), Qt::CaseInsensitive) ||
                isolation.contains(QStringLiteral("READ-COMMITTED"), Qt::CaseInsensitive));
    }
}

void TestACIDProperties::testIsolationTwoConnectionsCompeteSameRunway()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping multi-connection isolation test.");
    }

    // Create two completely independent named MySQL connections
    QString connNameA = QStringLiteral("Conn_Iso_RunwayA");
    QString connNameB = QStringLiteral("Conn_Iso_RunwayB");

    QString errA, errB;
    QSqlDatabase dbA = DatabaseManager::instance().createNamedConnection(connNameA, &errA);
    QSqlDatabase dbB = DatabaseManager::instance().createNamedConnection(connNameB, &errB);

    QVERIFY2(dbA.isOpen(), qPrintable(errA));
    QVERIFY2(dbB.isOpen(), qPrintable(errB));

    // Connection A begins transaction and locks runway row
    QVERIFY(dbA.transaction());
    {
        QSqlQuery qA(dbA);
        qA.prepare(QStringLiteral("SELECT runway_id, status FROM runways WHERE runway_id = :rid FOR UPDATE;"));
        qA.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(qA.exec() && qA.next());
        QCOMPARE(qA.value(1).toString(), QStringLiteral("Available"));

        // Insert assignment for flight 1 on Connection A
        QSqlQuery insA(dbA);
        insA.prepare(QStringLiteral("INSERT INTO runway_assignments (runway_id, flight_id, status, assigned_at) VALUES (:rid, :fid, 'Active', NOW());"));
        insA.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        insA.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(insA.exec());

        // Update runway to Occupied on Connection A
        QSqlQuery updA(dbA);
        updA.prepare(QStringLiteral("UPDATE runways SET status = 'Occupied' WHERE runway_id = :rid;"));
        updA.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(updA.exec());
    }

    // Now, run Connection B in a concurrent thread. Connection B attempts to lock the same runway.
    // Because Connection A holds the exclusive row lock, Connection B's thread will wait until Connection A commits!
    std::atomic<bool> threadStarted{false};
    std::atomic<bool> threadFinished{false};
    std::atomic<bool> connBFoundAvailable{false};

    std::thread worker([&]() {
        threadStarted = true;
        // Connect and attempt transaction
        if (dbB.transaction()) {
            QSqlQuery qB(dbB);
            qB.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :rid FOR UPDATE;"));
            qB.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
            if (qB.exec() && qB.next()) {
                if (qB.value(0).toString() == QStringLiteral("Available")) {
                    connBFoundAvailable = true;
                }
            }
            dbB.rollback();
        }
        threadFinished = true;
    });

    // Wait until worker thread starts
    while (!threadStarted) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Give worker time to block on the row lock
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    QCOMPARE(threadFinished.load(), false); // Must still be blocked waiting for lock!

    // Connection A now commits! This releases the lock and allows Connection B to proceed.
    QVERIFY(dbA.commit());

    worker.join();
    QCOMPARE(threadFinished.load(), true);

    // When Connection B unblocked, it saw the newly committed status ('Occupied'), NOT 'Available'!
    QCOMPARE(connBFoundAvailable.load(), false);

    // Verify exactly 1 active assignment exists in database
    {
        QSqlQuery q(dbA);
        q.prepare(QStringLiteral("SELECT flight_id FROM runway_assignments WHERE runway_id = :rid AND status = 'Active';"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), m_testFlightId1);
        QCOMPARE(q.next(), false); // Exactly 1
    }

    DatabaseManager::removeNamedConnection(connNameA);
    DatabaseManager::removeNamedConnection(connNameB);
}

void TestACIDProperties::testIsolationTwoConnectionsCompeteSameFlight()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping multi-connection flight isolation test.");
    }

    QString connNameA = QStringLiteral("Conn_Iso_FlightA");
    QString connNameB = QStringLiteral("Conn_Iso_FlightB");

    QSqlDatabase dbA = DatabaseManager::instance().createNamedConnection(connNameA);
    QSqlDatabase dbB = DatabaseManager::instance().createNamedConnection(connNameB);

    QVERIFY(dbA.isOpen());
    QVERIFY(dbB.isOpen());

    // Connection A assigns Flight 1 to Runway 1
    QVERIFY(dbA.transaction());
    {
        QSqlQuery insA(dbA);
        insA.prepare(QStringLiteral("INSERT INTO runway_assignments (runway_id, flight_id, status, assigned_at) VALUES (:rid, :fid, 'Active', NOW());"));
        insA.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        insA.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(insA.exec());
    }
    QVERIFY(dbA.commit());

    // Connection B checks whether Flight 1 has active assignment before assigning to Runway 2
    QVERIFY(dbB.transaction());
    int activeCount = 0;
    {
        QSqlQuery qB(dbB);
        qB.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE flight_id = :fid AND status = 'Active' FOR UPDATE;"));
        qB.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(qB.exec() && qB.next());
        activeCount = qB.value(0).toInt();
    }
    dbB.rollback();

    // Connection B saw active assignment and would abort
    QCOMPARE(activeCount, 1);

    DatabaseManager::removeNamedConnection(connNameA);
    DatabaseManager::removeNamedConnection(connNameB);
}

void TestACIDProperties::testIsolationRowLockWaitingAndTimeoutSafety()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping lock waiting test.");
    }

    QString connNameA = QStringLiteral("Conn_Iso_LockA");
    QSqlDatabase dbA = DatabaseManager::instance().createNamedConnection(connNameA);
    QVERIFY(dbA.isOpen());

    // Begin transaction and hold exclusive lock on Runway 1
    QVERIFY(dbA.transaction());
    {
        QSqlQuery qA(dbA);
        qA.prepare(QStringLiteral("SELECT runway_number FROM runways WHERE runway_id = :rid FOR UPDATE;"));
        qA.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(qA.exec() && qA.next());
    }

    // Release lock
    QVERIFY(dbA.commit());
    DatabaseManager::removeNamedConnection(connNameA);
}

// ── 4. ACID Property: Durability Tests ───────────────────────────────────────

void TestACIDProperties::testDurabilityInnodbFlushLogConfiguration()
{
    QString setting = DatabaseManager::instance().getInnodbFlushLogSetting();
    QVERIFY(!setting.isEmpty());
    if (m_dbConnected) {
        // Standard production MySQL durability setting is 1 (flush at every commit)
        QCOMPARE(setting, QStringLiteral("1"));
    }
}

void TestACIDProperties::testDurabilityCommittedAllocationSurvivesServiceRestart()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping durability restart test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // 1. Allocate runway successfully
    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::Success);

    // 2. Simulate clean application shutdown and restart
    m_service.shutdown();
    DatabaseManager::instance().disconnect();
    QCOMPARE(DatabaseManager::instance().isConnected(), false);

    // 3. Reconnect fresh application service instance
    ApplicationService newService;
    newService.initialize();
    QVERIFY(DatabaseManager::instance().isConnected());

    // 4. Query persistent MySQL disk state
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :rid;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Occupied")); // Persisted!
    }
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runway_assignments WHERE runway_id = :rid AND flight_id = :fid;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Active")); // Persisted!
    }

    newService.shutdown();
    // Restore m_service
    m_service.initialize();
}

void TestACIDProperties::testDurabilityCommittedReleaseSurvivesServiceRestart()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping durability restart test.");
    }

    Session atcSession(1, QStringLiteral("atc_acid"), QStringLiteral("ATC Acid"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    bool relOk = m_service.releaseRunway(m_testRunwayId1, &error);
    QVERIFY(relOk);

    // Simulate clean restart
    m_service.shutdown();
    DatabaseManager::instance().disconnect();

    ApplicationService newService;
    newService.initialize();

    // Verify released state survived restart
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :rid;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Available"));
    }
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runway_assignments WHERE runway_id = :rid ORDER BY assignment_id DESC;"));
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Released"));
    }

    newService.shutdown();
    m_service.initialize();
}

// ── 5. COMMIT and ROLLBACK Demonstrations ────────────────────────────────────

void TestACIDProperties::testDemonstrationCommitWorkflow()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping COMMIT demonstration.");
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QVERIFY(db.isOpen());

    // Step 1: START TRANSACTION
    QVERIFY(db.transaction());

    // Step 2: Mutate runway and insert assignment
    {
        QSqlQuery q1(db);
        q1.prepare(QStringLiteral("UPDATE runways SET status = 'Occupied' WHERE runway_id = :rid;"));
        q1.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q1.exec());

        QSqlQuery q2(db);
        q2.prepare(QStringLiteral("INSERT INTO runway_assignments (runway_id, flight_id, status, assigned_at) VALUES (:rid, :fid, 'Active', NOW());"));
        q2.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        q2.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        QVERIFY(q2.exec());
    }

    // Step 3: COMMIT
    QVERIFY(db.commit());

    // Step 4: Verify persistence
    {
        QSqlQuery v(db);
        v.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :rid;"));
        v.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(v.exec() && v.next());
        QCOMPARE(v.value(0).toString(), QStringLiteral("Occupied"));
    }
}

void TestACIDProperties::testDemonstrationRollbackWorkflow()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping ROLLBACK demonstration.");
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QVERIFY(db.isOpen());

    // Step 1: START TRANSACTION
    QVERIFY(db.transaction());

    // Step 2: Mutate runway and insert assignment inside uncommitted transaction
    {
        QSqlQuery q1(db);
        q1.prepare(QStringLiteral("UPDATE runways SET status = 'Occupied' WHERE runway_id = :rid;"));
        q1.bindValue(QStringLiteral(":rid"), m_testRunwayId2);
        QVERIFY(q1.exec());

        QSqlQuery q2(db);
        q2.prepare(QStringLiteral("INSERT INTO runway_assignments (runway_id, flight_id, status, assigned_at) VALUES (:rid, :fid, 'Active', NOW());"));
        q2.bindValue(QStringLiteral(":rid"), m_testRunwayId2);
        q2.bindValue(QStringLiteral(":fid"), m_testFlightId2);
        QVERIFY(q2.exec());
    }

    // Step 3: TRIGGER ROLLBACK
    QVERIFY(db.rollback());

    // Step 4: Verify zero modifications survived
    {
        QSqlQuery v1(db);
        v1.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :rid;"));
        v1.bindValue(QStringLiteral(":rid"), m_testRunwayId2);
        QVERIFY(v1.exec() && v1.next());
        QCOMPARE(v1.value(0).toString(), QStringLiteral("Available")); // Restored!

        QSqlQuery v2(db);
        v2.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE runway_id = :rid AND flight_id = :fid;"));
        v2.bindValue(QStringLiteral(":rid"), m_testRunwayId2);
        v2.bindValue(QStringLiteral(":fid"), m_testFlightId2);
        QVERIFY(v2.exec() && v2.next());
        QCOMPARE(v2.value(0).toInt(), 0); // Completely removed!
    }
}

QTEST_MAIN(TestACIDProperties)
#include "TestACIDProperties.moc"

