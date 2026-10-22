#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>

#include "models/Runway.h"
#include "models/Flight.h"
#include "models/Session.h"
#include "models/RunwayAssignment.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"

class TestRunwayResourceAllocation : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // ── 1. Model & Result Enum Tests ─────────────────────────────────────────
    void testModelAndResultEnumFormatting();

    // ── 2. RBAC & Service Boundary Authorization Tests ───────────────────────
    void testAllocationRejectedUnauthenticated();
    void testAllocationRejectedNonATC();
    void testAllocationRejectedInvalidInputs();

    // ── 3. Allocation Core Validation & Life Cycle (Live MySQL) ──────────────
    void testLiveSuccessfulAllocationToAvailableRunway();
    void testLiveAllocationRejectedWhenRunwayOccupied();
    void testLiveAllocationRejectedWhenRunwayInMaintenance();
    void testLiveAllocationRejectedUnknownRunwayId();
    void testLiveAllocationRejectedUnknownFlightId();
    void testLiveAllocationRejectedTerminalFlightStatus();
    void testLiveAllocationRejectedRunwayTypeMismatch();
    void testLiveAllocationRejectedDuplicateActiveRunwayAssignment();
    void testLiveAllocationRejectedConflictingActiveFlightAssignment();
    void testLiveAllocationRejectedCompletedQueueProcess();

    // ── 4. Release Operation & State Consistency ─────────────────────────────
    void testLiveReleaseRunwayLifecycle();
    void testLiveQueriesEligibleFlightsAndAvailableRunways();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;

    // Test fixture entity IDs
    int m_testRunwayId1 = 0; // Both (default available)
    int m_testRunwayId2 = 0; // Landing-only
    int m_testRunwayId3 = 0; // Takeoff-only
    int m_testFlightId1 = 0; // Arrival Scheduled
    int m_testFlightId2 = 0; // Departure Scheduled
};

void TestRunwayResourceAllocation::initTestCase()
{
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();
    if (!m_dbConnected) {
        qWarning() << "[TestRunwayResourceAllocation] Live database unavailable; database tests will be skipped.";
    }
}

void TestRunwayResourceAllocation::cleanupTestCase()
{
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            // Delete temporary assignments, logs, queue, flights, runways created for tests
            q.exec(QStringLiteral("DELETE FROM runway_assignments WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'RA-T%');"));
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'RA-T%');"));
            q.exec(QStringLiteral("DELETE FROM operation_logs WHERE message LIKE '%RA-T%' OR message LIKE '%RW-AL%';"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'RA-T%';"));
            q.exec(QStringLiteral("DELETE FROM runways WHERE runway_number LIKE 'RW-AL%';"));
        }
    }
    m_service.shutdown();
}

void TestRunwayResourceAllocation::init()
{
    if (!m_dbConnected) return;

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) return;

    cleanup();

    // 1. Create test runways (runway_number must be <= 10 characters)
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO runways (runway_number, runway_type, status) VALUES ('RW-AL01', 'Both', 'Available');"));
    if (q.exec()) {
        m_testRunwayId1 = q.lastInsertId().toInt();
    } else {
        qWarning() << "Failed to create RW-AL01:" << q.lastError().text();
    }

    q.prepare(QStringLiteral("INSERT INTO runways (runway_number, runway_type, status) VALUES ('RW-AL02', 'Landing', 'Available');"));
    if (q.exec()) {
        m_testRunwayId2 = q.lastInsertId().toInt();
    } else {
        qWarning() << "Failed to create RW-AL02:" << q.lastError().text();
    }

    q.prepare(QStringLiteral("INSERT INTO runways (runway_number, runway_type, status) VALUES ('RW-AL03', 'Takeoff', 'Available');"));
    if (q.exec()) {
        m_testRunwayId3 = q.lastInsertId().toInt();
    } else {
        qWarning() << "Failed to create RW-AL03:" << q.lastError().text();
    }

    // 2. Create test flights
    q.prepare(QStringLiteral(
        "INSERT INTO flights (flight_number, airline, source, destination, flight_type, priority, operational_status, scheduled_time) "
        "VALUES ('RA-T01', 'TestAir', 'DEL', 'BOM', 'Arrival', 2, 'Scheduled', NOW());"
    ));
    if (q.exec()) {
        m_testFlightId1 = q.lastInsertId().toInt();
    } else {
        qWarning() << "Failed to create RA-T01:" << q.lastError().text();
    }

    q.prepare(QStringLiteral(
        "INSERT INTO flights (flight_number, airline, source, destination, flight_type, priority, operational_status, scheduled_time) "
        "VALUES ('RA-T02', 'TestAir', 'BOM', 'BLR', 'Departure', 3, 'Scheduled', NOW());"
    ));
    if (q.exec()) {
        m_testFlightId2 = q.lastInsertId().toInt();
    } else {
        qWarning() << "Failed to create RA-T02:" << q.lastError().text();
    }
}

void TestRunwayResourceAllocation::cleanup()
{
    if (!m_dbConnected) return;

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.exec(QStringLiteral("DELETE FROM runway_assignments WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'RA-T%');"));
    q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'RA-T%');"));
    q.exec(QStringLiteral("DELETE FROM operation_logs WHERE message LIKE '%RA-T%' OR message LIKE '%RW-AL%';"));
    q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'RA-T%';"));
    q.exec(QStringLiteral("DELETE FROM runways WHERE runway_number LIKE 'RW-AL%';"));

    m_testRunwayId1 = 0;
    m_testRunwayId2 = 0;
    m_testRunwayId3 = 0;
    m_testFlightId1 = 0;
    m_testFlightId2 = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Model & Result Enum Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestRunwayResourceAllocation::testModelAndResultEnumFormatting()
{
    Models::RunwayAssignment ra(10, 20, 30, 40, QStringLiteral("Active"));
    QCOMPARE(ra.assignmentId, 10);
    QCOMPARE(ra.flightId, 20);
    QCOMPARE(ra.runwayId, 30);
    QCOMPARE(ra.assignedBy, 40);
    QCOMPARE(ra.status, QStringLiteral("Active"));
    QVERIFY(ra.isActive());
    QCOMPARE(ra.isReleased(), false);
    QCOMPARE(ra.isCancelled(), false);

    ra.status = QStringLiteral("Released");
    QCOMPARE(ra.isActive(), false);
    QVERIFY(ra.isReleased());

    ra.status = QStringLiteral("Cancelled");
    QVERIFY(ra.isCancelled());

    // Verify error strings
    QVERIFY(!Models::allocationResultToString(Models::AllocationResult::Success).isEmpty());
    QVERIFY(Models::allocationResultToString(Models::AllocationResult::RunwayUnavailable).contains(QStringLiteral("unavailable")));
    QVERIFY(Models::allocationResultToString(Models::AllocationResult::FlightIneligible).contains(QStringLiteral("ineligible")));
    QVERIFY(Models::allocationResultToString(Models::AllocationResult::ConflictExistingAssignment).contains(QStringLiteral("conflict")));
    QVERIFY(Models::allocationResultToString(Models::AllocationResult::Unauthorized).contains(QStringLiteral("unauthorized")));
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. RBAC & Service Boundary Authorization Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestRunwayResourceAllocation::testAllocationRejectedUnauthenticated()
{
    m_service.logout();
    QString error;

    Models::AllocationResult res = m_service.allocateRunway(1, 1, &error);
    QCOMPARE(res, Models::AllocationResult::Unauthorized);
    QVERIFY(error.contains(QStringLiteral("Authentication required")));

    bool boolRes = m_service.allocateRunwayResource(1, 1, &error);
    QCOMPARE(boolRes, false);

    bool releaseRes = m_service.releaseRunway(1, &error);
    QCOMPARE(releaseRes, false);
    QVERIFY(error.contains(QStringLiteral("Authentication required")));
}

void TestRunwayResourceAllocation::testAllocationRejectedNonATC()
{
    QString error;

    // 1. Airport Operator role must be rejected for allocation
    Session operatorSession(1, QStringLiteral("operator"), QStringLiteral("Op User"), QStringLiteral("Airport Operator"));
    m_service.setTestSession(operatorSession);

    Models::AllocationResult resOp = m_service.allocateRunway(1, 1, &error);
    QCOMPARE(resOp, Models::AllocationResult::Unauthorized);
    QVERIFY(error.contains(QStringLiteral("Access denied")));

    // 2. Admin role must also be rejected for allocation (action:RunwayAllocation is strictly ATC-only)
    Session adminSession(2, QStringLiteral("admin"), QStringLiteral("Admin User"), QStringLiteral("Admin"));
    m_service.setTestSession(adminSession);

    error.clear();
    Models::AllocationResult resAdmin = m_service.allocateRunway(1, 1, &error);
    QCOMPARE(resAdmin, Models::AllocationResult::Unauthorized);
    QVERIFY(error.contains(QStringLiteral("Access denied")));
}

void TestRunwayResourceAllocation::testAllocationRejectedInvalidInputs()
{
    Session atcSession(3, QStringLiteral("atc"), QStringLiteral("ATC User"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    QCOMPARE(m_service.allocateRunway(0, 1, &error), Models::AllocationResult::InvalidInput);
    QCOMPARE(m_service.allocateRunway(-5, 1, &error), Models::AllocationResult::InvalidInput);
    QCOMPARE(m_service.allocateRunway(1, 0, &error), Models::AllocationResult::InvalidInput);
    QCOMPARE(m_service.allocateRunway(1, -2, &error), Models::AllocationResult::InvalidInput);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Allocation Core Validation & Life Cycle (Live MySQL)
// ─────────────────────────────────────────────────────────────────────────────

void TestRunwayResourceAllocation::testLiveSuccessfulAllocationToAvailableRunway()
{
    if (!m_dbConnected) QSKIP("Live MySQL required for database tests.");

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QVERIFY2(res == Models::AllocationResult::Success, qPrintable(error));

    // Verify runway updated to Occupied
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery qRw(db);
    qRw.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :id;"));
    qRw.bindValue(QStringLiteral(":id"), m_testRunwayId1);
    QVERIFY(qRw.exec() && qRw.next());
    QCOMPARE(qRw.value(0).toString(), QStringLiteral("Occupied"));

    // Verify flight updated from Scheduled to Cleared
    QSqlQuery qF(db);
    qF.prepare(QStringLiteral("SELECT operational_status FROM flights WHERE flight_id = :id;"));
    qF.bindValue(QStringLiteral(":id"), m_testFlightId1);
    QVERIFY(qF.exec() && qF.next());
    QCOMPARE(qF.value(0).toString(), QStringLiteral("Cleared"));

    // Verify runway_assignments record inserted
    QSqlQuery qRa(db);
    qRa.prepare(QStringLiteral("SELECT status, assigned_by FROM runway_assignments WHERE runway_id = :rid AND flight_id = :fid;"));
    qRa.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
    qRa.bindValue(QStringLiteral(":fid"), m_testFlightId1);
    QVERIFY(qRa.exec() && qRa.next());
    QCOMPARE(qRa.value(0).toString(), QStringLiteral("Active"));
    QCOMPARE(qRa.value(1).toInt(), 2);
}

void TestRunwayResourceAllocation::testLiveAllocationRejectedWhenRunwayOccupied()
{
    if (!m_dbConnected) QSKIP("Live MySQL required for database tests.");

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Set runway to Occupied
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE runways SET status = 'Occupied' WHERE runway_id = :id;"));
    q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
    QVERIFY(q.exec());

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::RunwayUnavailable);
    QVERIFY(error.contains(QStringLiteral("Occupied")));
}

void TestRunwayResourceAllocation::testLiveAllocationRejectedWhenRunwayInMaintenance()
{
    if (!m_dbConnected) QSKIP("Live MySQL required for database tests.");

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Set runway to Maintenance
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE runways SET status = 'Maintenance' WHERE runway_id = :id;"));
    q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
    QVERIFY(q.exec());

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::RunwayUnavailable);
    QVERIFY(error.contains(QStringLiteral("Maintenance")));
}

void TestRunwayResourceAllocation::testLiveAllocationRejectedUnknownRunwayId()
{
    if (!m_dbConnected) QSKIP("Live MySQL required for database tests.");

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(999999, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::RunwayNotFound);
    QVERIFY(error.contains(QStringLiteral("not found")));
}

void TestRunwayResourceAllocation::testLiveAllocationRejectedUnknownFlightId()
{
    if (!m_dbConnected) QSKIP("Live MySQL required for database tests.");

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, 999999, &error);
    QCOMPARE(res, Models::AllocationResult::FlightNotFound);
    QVERIFY(error.contains(QStringLiteral("not found")));
}

void TestRunwayResourceAllocation::testLiveAllocationRejectedTerminalFlightStatus()
{
    if (!m_dbConnected) QSKIP("Live MySQL required for database tests.");

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);

    const QStringList terminalStatuses = {
        QStringLiteral("Landed"),
        QStringLiteral("Departed"),
        QStringLiteral("Cancelled")
    };

    for (const QString &status : terminalStatuses) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE flights SET operational_status = :status WHERE flight_id = :id;"));
        q.bindValue(QStringLiteral(":status"), status);
        q.bindValue(QStringLiteral(":id"), m_testFlightId1);
        QVERIFY(q.exec());

        QString error;
        Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
        QCOMPARE(res, Models::AllocationResult::FlightIneligible);
        QVERIFY2(error.contains(status), qPrintable(error));
    }
}

void TestRunwayResourceAllocation::testLiveAllocationRejectedRunwayTypeMismatch()
{
    if (!m_dbConnected) QSKIP("Live MySQL required for database tests.");

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;

    // Runway 2 is Landing-only; flight 2 is Departure. Must be rejected.
    Models::AllocationResult res1 = m_service.allocateRunway(m_testRunwayId2, m_testFlightId2, &error);
    QCOMPARE(res1, Models::AllocationResult::FlightIneligible);
    QVERIFY(error.contains(QStringLiteral("Landing only")));

    // Runway 3 is Takeoff-only; flight 1 is Arrival. Must be rejected.
    error.clear();
    Models::AllocationResult res2 = m_service.allocateRunway(m_testRunwayId3, m_testFlightId1, &error);
    QCOMPARE(res2, Models::AllocationResult::FlightIneligible);
    QVERIFY(error.contains(QStringLiteral("Takeoff only")));
}

void TestRunwayResourceAllocation::testLiveAllocationRejectedDuplicateActiveRunwayAssignment()
{
    if (!m_dbConnected) QSKIP("Live MySQL required for database tests.");

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Insert an active assignment on runway 1 directly
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO runway_assignments (flight_id, runway_id, assigned_by, status) VALUES (:fid, :rid, 1, 'Active');"));
    q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
    q.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
    QVERIFY(q.exec());

    // Even if runway status was Available in runways table, duplicate check in runway_assignments catches it
    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId2, &error);
    QCOMPARE(res, Models::AllocationResult::ConflictExistingAssignment);
    QVERIFY(error.contains(QStringLiteral("already has an active assignment")));
}

void TestRunwayResourceAllocation::testLiveAllocationRejectedConflictingActiveFlightAssignment()
{
    if (!m_dbConnected) QSKIP("Live MySQL required for database tests.");

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Insert an active assignment on runway 2 for flight 1
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO runway_assignments (flight_id, runway_id, assigned_by, status) VALUES (:fid, :rid, 1, 'Active');"));
    q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
    q.bindValue(QStringLiteral(":rid"), m_testRunwayId2);
    QVERIFY(q.exec());

    // Attempting to allocate flight 1 to runway 1 must be rejected
    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::ConflictExistingAssignment);
    QVERIFY(error.contains(QStringLiteral("already possesses an active assignment")));
}

void TestRunwayResourceAllocation::testLiveAllocationRejectedCompletedQueueProcess()
{
    if (!m_dbConnected) QSKIP("Live MySQL required for database tests.");

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Insert completed scheduling_queue record for flight 1
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO scheduling_queue (flight_id, base_priority, effective_priority, service_time, process_state) VALUES (:fid, 2, 2, 60, 'Completed');"));
    q.bindValue(QStringLiteral(":fid"), m_testFlightId1);
    QVERIFY(q.exec());

    QString error;
    Models::AllocationResult res = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(res, Models::AllocationResult::FlightIneligible);
    QVERIFY(error.contains(QStringLiteral("Completed state")));
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Release Operation & State Consistency
// ─────────────────────────────────────────────────────────────────────────────

void TestRunwayResourceAllocation::testLiveReleaseRunwayLifecycle()
{
    if (!m_dbConnected) QSKIP("Live MySQL required for database tests.");

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    Models::AllocationResult allocRes = m_service.allocateRunway(m_testRunwayId1, m_testFlightId1, &error);
    QCOMPARE(allocRes, Models::AllocationResult::Success);

    // Verify runway is Occupied
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :id;"));
    q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
    QVERIFY(q.exec() && q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("Occupied"));

    // Release the runway
    bool releaseOk = m_service.releaseRunway(m_testRunwayId1, &error);
    QVERIFY2(releaseOk, qPrintable(error));

    // Verify runway is Available
    q.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :id;"));
    q.bindValue(QStringLiteral(":id"), m_testRunwayId1);
    QVERIFY(q.exec() && q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("Available"));

    // Verify assignment is Released and released_at is set
    QSqlQuery qRa(db);
    qRa.prepare(QStringLiteral("SELECT status, released_at FROM runway_assignments WHERE runway_id = :rid AND flight_id = :fid;"));
    qRa.bindValue(QStringLiteral(":rid"), m_testRunwayId1);
    qRa.bindValue(QStringLiteral(":fid"), m_testFlightId1);
    QVERIFY(qRa.exec() && qRa.next());
    QCOMPARE(qRa.value(0).toString(), QStringLiteral("Released"));
    QVERIFY(!qRa.value(1).toDateTime().isNull());
}

void TestRunwayResourceAllocation::testLiveQueriesEligibleFlightsAndAvailableRunways()
{
    if (!m_dbConnected) QSKIP("Live MySQL required for database tests.");

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("ATC Demo"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    QList<Models::Runway> avail = m_service.getAvailableRunwaysForAllocation(&error);
    QVERIFY(avail.size() >= 3);

    QList<Models::Flight> eligible = m_service.getEligibleFlightsForRunwayAllocation(m_testRunwayId1, &error);
    QVERIFY(eligible.size() >= 2);

    // Test active assignment queries before and after allocation
    Models::RunwayAssignment asgn;
    QCOMPARE(m_service.getActiveAssignmentForRunway(m_testRunwayId1, asgn), false);

    m_service.allocateRunway(m_testRunwayId1, m_testFlightId1);
    QVERIFY(m_service.getActiveAssignmentForRunway(m_testRunwayId1, asgn));
    QCOMPARE(asgn.runwayId, m_testRunwayId1);
    QCOMPARE(asgn.flightId, m_testFlightId1);

    Models::RunwayAssignment flightAsgn;
    QVERIFY(m_service.getActiveAssignmentForFlight(m_testFlightId1, flightAsgn));
    QCOMPARE(flightAsgn.runwayId, m_testRunwayId1);
}

QTEST_MAIN(TestRunwayResourceAllocation)
#include "TestRunwayResourceAllocation.moc"
