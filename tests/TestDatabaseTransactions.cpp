#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDriver>

#include "models/Runway.h"
#include "models/Flight.h"
#include "models/Session.h"
#include "models/RunwayAssignment.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"
#include "resource/ResourceManager.h"

/**
 * @brief Comprehensive automated test suite for Phase 2 / Day 21 MySQL Transactions.
 *
 * Verifies:
 *   1. Transaction driver capability and connectivity.
 *   2. RBAC & boundary validation for transactional operations.
 *   3. In-memory state rollback consistency (ResourceManager).
 *   4. Atomic Runway Allocation (COMMIT path):
 *      - All four mutations (runway, flight, assignment, log) commit together.
 *   5. Atomic Runway Allocation (ROLLBACK path):
 *      - Rejection on conflict or ineligible state aborts transaction without partial mutations.
 *   6. Atomic Runway Release (COMMIT and ROLLBACK paths).
 *   7. Intermediate failure rollback leaving zero orphaned assignment records.
 */
class TestDatabaseTransactions : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // ── 1. Unit, RBAC, and Validation Tests ──────────────────────────────────
    void testDriverTransactionCapability();
    void testAllocationRequiresAuthentication();
    void testAllocationRequiresATCRole();
    void testAllocationRejectsInvalidInputs();
    void testReleaseRequiresAuthentication();
    void testReleaseRequiresAuthorizedRole();
    void testReleaseRejectsInvalidRunwayId();
    void testOfflineDatabaseHandling();
    void testResourceManagerRollbackIntegrity();

    // ── 2. Live MySQL Transaction Integration Tests ─────────────────────────
    void testLiveAtomicAllocationSuccess();
    void testLiveAtomicAllocationRollbackOnDuplicateRunway();
    void testLiveAtomicAllocationRollbackOnTerminalFlight();
    void testLiveAtomicReleaseSuccess();
    void testLiveAtomicReleaseRollbackOnMaintenance();
    void testLiveSimulatedTransactionRollbackLeavesNoOrphanRecords();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;

    // Test fixture entity IDs
    int m_testRunwayId1 = 0;
    int m_testRunwayId2 = 0;
    int m_testFlightId1 = 0;
    int m_testFlightId2 = 0;
    int m_testFlightId3 = 0;
};

void TestDatabaseTransactions::initTestCase()
{
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();
    if (!m_dbConnected) {
        qWarning() << "[TestDatabaseTransactions] Live database connection is offline. Integration tests will be skipped via QSKIP.";
    }
}

void TestDatabaseTransactions::cleanupTestCase()
{
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM runway_assignments WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'TX-T%');"));
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'TX-T%');"));
            q.exec(QStringLiteral("DELETE FROM operation_logs WHERE message LIKE '%TX-T%' OR message LIKE '%RW-TX%';"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'TX-T%';"));
            q.exec(QStringLiteral("DELETE FROM runways WHERE runway_number LIKE 'RW-TX%';"));
        }
    }
    m_service.shutdown();
}

void TestDatabaseTransactions::init()
{
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
        q.prepare(QStringLiteral("INSERT INTO runways (runway_number, runway_type, status) VALUES ('RW-TX01', 'Both', 'Available');"));
        if (q.exec()) {
            m_testRunwayId1 = q.lastInsertId().toInt();
        }
    }

    // Fixture Runway 2: Available, Landing
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("INSERT INTO runways (runway_number, runway_type, status) VALUES ('RW-TX02', 'Landing', 'Available');"));
        if (q.exec()) {
            m_testRunwayId2 = q.lastInsertId().toInt();
        }
    }

    // Fixture Flight 1: Arrival, Scheduled
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT INTO flights (flight_number, airline, flight_type, operational_status, scheduled_time, emergency_status) "
            "VALUES ('TX-T101', 'Air India', 'Arrival', 'Scheduled', NOW(), 0);"
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
            "VALUES ('TX-T102', 'IndiGo', 'Departure', 'Scheduled', NOW(), 0);"
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
            "VALUES ('TX-T103', 'SpiceJet', 'Arrival', 'Landed', NOW(), 0);"
        ));
        if (q.exec()) {
            m_testFlightId3 = q.lastInsertId().toInt();
        }
    }
}

void TestDatabaseTransactions::cleanup()
{
    if (!m_dbConnected) {
        return;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        return;
    }

    QSqlQuery q(db);
    q.exec(QStringLiteral("DELETE FROM runway_assignments WHERE runway_id IN (SELECT runway_id FROM runways WHERE runway_number LIKE 'RW-TX%');"));
    q.exec(QStringLiteral("DELETE FROM operation_logs WHERE message LIKE '%TX-T%' OR message LIKE '%RW-TX%';"));
    q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'TX-T%';"));
    q.exec(QStringLiteral("DELETE FROM runways WHERE runway_number LIKE 'RW-TX%';"));

    m_testRunwayId1 = 0;
    m_testRunwayId2 = 0;
    m_testFlightId1 = 0;
    m_testFlightId2 = 0;
    m_testFlightId3 = 0;

    if (m_service.resourceManager()) {
        m_service.resourceManager()->clearState();
    }
}

// ── 1. Unit, RBAC, and Validation Tests ──────────────────────────────────────

void TestDatabaseTransactions::testDriverTransactionCapability()
{
    // Verify QMYSQL driver availability
    bool driverAvail = DatabaseManager::isDriverAvailable();
    QVERIFY(driverAvail);

    // If database is connected, check transaction feature
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        QVERIFY(db.isOpen());
        QVERIFY(db.driver() != nullptr);
        QVERIFY(db.driver()->hasFeature(QSqlDriver::Transactions));
    }
}

void TestDatabaseTransactions::testAllocationRequiresAuthentication()
{
    m_service.logout();
    QCOMPARE(m_service.isAuthenticated(), false);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(1, 1, &error);
    QCOMPARE(res, Models::AllocationResult::Unauthorized);
    QVERIFY(error.contains(QStringLiteral("Authentication required"), Qt::CaseInsensitive));
}

void TestDatabaseTransactions::testAllocationRequiresATCRole()
{
    // Test with Airport Operator (unauthorized for allocation)
    Session operatorSession(2, QStringLiteral("operator_user"), QStringLiteral("Op User"), QStringLiteral("Airport Operator"));
    m_service.setTestSession(operatorSession);
    QVERIFY(m_service.isAuthenticated());

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(1, 1, &error);
    QCOMPARE(res, Models::AllocationResult::Unauthorized);
    QVERIFY(error.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));

    m_service.logout();
}

void TestDatabaseTransactions::testAllocationRejectsInvalidInputs()
{
    Session atcSession(1, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    // Zero runway ID
    Models::AllocationResult res1 = m_service.allocateRunway(0, 5, &error);
    QCOMPARE(res1, Models::AllocationResult::InvalidInput);

    // Negative flight ID
    Models::AllocationResult res2 = m_service.allocateRunway(5, -1, &error);
    QCOMPARE(res2, Models::AllocationResult::InvalidInput);

    m_service.logout();
}

void TestDatabaseTransactions::testReleaseRequiresAuthentication()
{
    m_service.logout();
    QCOMPARE(m_service.isAuthenticated(), false);

    QString error;
    bool ok = m_service.releaseRunway(1, &error);
    QCOMPARE(ok, false);
    QVERIFY(error.contains(QStringLiteral("Authentication required"), Qt::CaseInsensitive));
}

void TestDatabaseTransactions::testReleaseRequiresAuthorizedRole()
{
    // Viewer role has no release permission
    Session viewerSession(3, QStringLiteral("viewer_user"), QStringLiteral("Viewer User"), QStringLiteral("Flight Viewer"));
    m_service.setTestSession(viewerSession);

    QString error;
    bool ok = m_service.releaseRunway(1, &error);
    QCOMPARE(ok, false);
    QVERIFY(error.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));

    m_service.logout();
}

void TestDatabaseTransactions::testReleaseRejectsInvalidRunwayId()
{
    Session atcSession(1, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    bool ok = m_service.releaseRunway(0, &error);
    QCOMPARE(ok, false);
    QVERIFY(error.contains(QStringLiteral("Invalid runway ID"), Qt::CaseInsensitive));

    m_service.logout();
}

void TestDatabaseTransactions::testOfflineDatabaseHandling()
{
    if (m_dbConnected) {
        QSKIP("Live database is currently connected; offline handling test applies to disconnected environments.");
    }

    Session atcSession(1, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(10, 20, &error);
    QCOMPARE(res, Models::AllocationResult::DatabaseError);
    QVERIFY(error.contains(QStringLiteral("offline"), Qt::CaseInsensitive));

    m_service.logout();
}

void TestDatabaseTransactions::testResourceManagerRollbackIntegrity()
{
    ResourceManager *mgr = m_service.resourceManager();
    QVERIFY(mgr != nullptr);

    mgr->clearState();
    QCOMPARE(mgr->isRunwayReserved(42), false);
    QCOMPARE(mgr->activeRunwayForFlight(100), 0);

    // Simulate in-memory reservation
    mgr->confirmAllocation(42, 100);
    QCOMPARE(mgr->isRunwayReserved(42), true);
    QCOMPARE(mgr->activeFlightForRunway(42), 100);
    QCOMPARE(mgr->activeRunwayForFlight(100), 42);

    // Rollback reservation
    mgr->rollbackAllocation(42, 100);
    QCOMPARE(mgr->isRunwayReserved(42), false);
    QCOMPARE(mgr->activeFlightForRunway(42), 0);
    QCOMPARE(mgr->activeRunwayForFlight(100), 0);
}

// ── 2. Live MySQL Transaction Integration Tests ─────────────────────────────

void TestDatabaseTransactions::testLiveAtomicAllocationSuccess()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live allocation transaction test.");
    }

    QVERIFY(m_testRunwayId1 > 0);
    QVERIFY(m_testFlightId1 > 0);

    Session atcSession(1, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::Success);

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);

    // 1. Verify runway status is 'Occupied'
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Occupied"));
    }

    // 2. Verify flight status is 'Cleared'
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT operational_status FROM flights WHERE flight_id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_testFlightId1);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Cleared"));
    }

    // 3. Verify runway_assignments contains exactly 1 active record
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status, flight_id FROM runway_assignments WHERE runway_id = :id AND status = 'Active';"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Active"));
        QCOMPARE(q.value(1).toInt(), m_testFlightId1);
    }

    // 4. Verify operation_logs contains RUNWAY_ASSIGN with transaction_status 'Committed'
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT transaction_status FROM operation_logs WHERE runway_id = :id AND operation_type = 'RUNWAY_ASSIGN' ORDER BY log_id DESC;"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Committed"));
    }

    m_service.logout();
}

void TestDatabaseTransactions::testLiveAtomicAllocationRollbackOnDuplicateRunway()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping rollback on duplicate runway test.");
    }

    Session atcSession(1, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Allocate runway to Flight 1
    QString error;
    Models::AllocationResult res1 = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res1, Models::AllocationResult::Success);

    // Attempt second allocation of same runway to Flight 2
    Models::AllocationResult res2 = m_service.allocateRunway(m_testRunwayId1, m_testFlightId2, &error);
    QCOMPARE(res2, Models::AllocationResult::ConflictExistingAssignment);

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);

    // Verify Flight 2 remains in 'Scheduled' state (not Cleared)
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT operational_status FROM flights WHERE flight_id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_testFlightId2);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Scheduled"));
    }

    // Verify runway is still assigned ONLY to Flight 1
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT flight_id FROM runway_assignments WHERE runway_id = :id AND status = 'Active';"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), m_testFlightId1);
        QCOMPARE(q.next(), false); // Exactly 1 active assignment
    }

    m_service.logout();
}

void TestDatabaseTransactions::testLiveAtomicAllocationRollbackOnTerminalFlight()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping terminal flight rollback test.");
    }

    Session atcSession(1, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Flight 3 is 'Landed' (terminal status)
    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId2, m_testFlightId3, &error);
    QCOMPARE(res, Models::AllocationResult::FlightIneligible);

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);

    // Verify runway remains Available
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId2);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Available"));
    }

    // Verify no assignment was inserted
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE runway_id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId2);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }

    m_service.logout();
}

void TestDatabaseTransactions::testLiveAtomicReleaseSuccess()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping live runway release transaction test.");
    }

    Session atcSession(1, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Allocate first
    QString error;
    Models::AllocationResult allocRes = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(allocRes, Models::AllocationResult::Success);

    // Release
    bool released = m_service.releaseRunway(m_testRunwayId1, &error);
    QVERIFY(released);

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);

    // 1. Verify runway status returned to Available
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Available"));
    }

    // 2. Verify assignment status is 'Released' with non-null released_at
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status, released_at FROM runway_assignments WHERE runway_id = :id ORDER BY assignment_id DESC;"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Released"));
        QVERIFY(!q.value(1).isNull());
    }

    // 3. Verify operation_logs contains RUNWAY_RELEASE with transaction_status 'Committed'
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT transaction_status FROM operation_logs WHERE runway_id = :id AND operation_type = 'RUNWAY_RELEASE' ORDER BY log_id DESC;"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Committed"));
    }

    m_service.logout();
}

void TestDatabaseTransactions::testLiveAtomicReleaseRollbackOnMaintenance()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping maintenance release rollback test.");
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);

    // Put runway into Maintenance directly
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE runways SET status = 'Maintenance' WHERE runway_id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
        QVERIFY(q.exec());
    }

    Session atcSession(1, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    bool released = m_service.releaseRunway(m_testRunwayId1, &error);
    QCOMPARE(released, false);
    QVERIFY(error.contains(QStringLiteral("Maintenance"), Qt::CaseInsensitive));

    // Verify runway is still in Maintenance
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Maintenance"));
    }

    m_service.logout();
}

void TestDatabaseTransactions::testLiveSimulatedTransactionRollbackLeavesNoOrphanRecords()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database is offline; skipping simulated transaction rollback test.");
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QVERIFY(db.isOpen());

    // Begin explicit transaction
    bool inTx = db.transaction();
    QVERIFY(inTx);

    // Mutate runway to Occupied
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE runways SET status = 'Occupied' WHERE runway_id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
        QVERIFY(q.exec());
    }

    // Insert an assignment record
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("INSERT INTO runway_assignments (flight_id, runway_id, assigned_at, status) VALUES (:fid, :rid, NOW(), 'Active');"));
        q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
        q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
        QVERIFY(q.exec());
    }

    // Force rollback
    bool rolledBack = db.rollback();
    QVERIFY(rolledBack);

    // Verify that runway is still 'Available' and zero assignment records exist
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Available"));
    }

    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE runway_id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }
}

QTEST_MAIN(TestDatabaseTransactions)
#include "TestDatabaseTransactions.moc"

