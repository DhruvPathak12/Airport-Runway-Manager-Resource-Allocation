#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>

#include "models/Process.h"
#include "models/Flight.h"
#include "models/Session.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"

class TestProcessModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ── 1. Model & Validation Tests (Unit Tests - No DB required) ────────────
    void testDefaultConstructionAndInitialization();
    void testSetAndGetIdentities();
    void testValidPriorityBoundaries();
    void testPriorityOutOfRangeRejection();
    void testServiceTimeValidation();
    void testBaseAndEffectivePrioritySeparation();
    void testProcessStateRepresentationAndMapping();
    void testFlightToProcessMapping();
    void testFlightEligibilityRules();
    void testMetricsRemainUncalculated();

    // ── 2. Service Boundary & Role-Based Access Control ──────────────────────
    void testUnauthenticatedCannotEnqueueFlight();
    void testOperatorRoleDeniedProcessCreation();
    void testATCRolePermittedProcessCreation();
    void testAdminRolePermittedProcessCreation();

    // ── 3. Live MySQL Persistence & Duplicate Prevention ─────────────────────
    void testLiveProcessCreationAndRetrieval();
    void testLiveDuplicateProcessRejection();
    void testLiveIneligibleFlightProcessCreationRejection();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;
    int                m_testFlightId1 = 0;
    int                m_testFlightId2 = 0;
};

void TestProcessModel::initTestCase()
{
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();
    if (!m_dbConnected) {
        qWarning() << "[TestProcessModel] Live database connection unavailable; testing offline unit functionality only.";
    } else {
        // Clean up any stale test rows first
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'PCB-%');"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'PCB-%';"));

            // Create test flights for live tests
            m_service.setTestSession(Session(1, QStringLiteral("atc_tester"), QStringLiteral("ATC Tester"), QStringLiteral("ATC")));

            Models::Flight f1;
            f1.flightNumber = QStringLiteral("PCB-101");
            f1.airline = QStringLiteral("Test Air");
            f1.source = QStringLiteral("DEL");
            f1.destination = QStringLiteral("BOM");
            f1.flightType = QStringLiteral("Arrival");
            f1.scheduledTime = QDateTime::currentDateTime().addSecs(1800);
            f1.priority = 2;
            f1.operationalStatus = QStringLiteral("Scheduled");

            if (m_service.createFlight(f1)) {
                QList<Models::Flight> flist = m_service.getAllFlights(QStringLiteral("PCB-101"));
                if (!flist.isEmpty()) {
                    m_testFlightId1 = flist.first().id;
                }
            }

            Models::Flight f2;
            f2.flightNumber = QStringLiteral("PCB-102");
            f2.airline = QStringLiteral("Test Air");
            f2.source = QStringLiteral("DEL");
            f2.destination = QStringLiteral("BLR");
            f2.flightType = QStringLiteral("Departure");
            f2.scheduledTime = QDateTime::currentDateTime().addSecs(3600);
            f2.priority = 4;
            f2.operationalStatus = QStringLiteral("Landed"); // Ineligible terminal status

            if (m_service.createFlight(f2)) {
                QList<Models::Flight> flist = m_service.getAllFlights(QStringLiteral("PCB-102"));
                if (!flist.isEmpty()) {
                    m_testFlightId2 = flist.first().id;
                }
            }
        }
    }
}

void TestProcessModel::cleanupTestCase()
{
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM scheduling_queue WHERE flight_id IN (SELECT flight_id FROM flights WHERE flight_number LIKE 'PCB-%');"));
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'PCB-%';"));
        }
    }
    m_service.shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Model & Validation Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestProcessModel::testDefaultConstructionAndInitialization()
{
    Models::Process proc;

    // Verify default identity values
    QCOMPARE(proc.id, 0);
    QCOMPARE(proc.queueId, 0);
    QCOMPARE(proc.flightId, 0);
    QCOMPARE(proc.processId(), 0);
    QVERIFY(proc.isNew());
    QVERIFY(!proc.isPersisted());

    // Verify default scheduling attributes
    QVERIFY(proc.arrivalTime.isValid());
    QCOMPARE(proc.basePriority, 3);
    QCOMPARE(proc.effectivePriority, 3);
    QCOMPARE(proc.serviceTime, 60);
    QCOMPARE(proc.burstTime(), 60);

    // Verify metrics are initially uncalculated
    QCOMPARE(proc.waitingTime, -1);
    QCOMPARE(proc.turnaroundTime, -1);
    QVERIFY(!proc.completionTime.isValid());
    QVERIFY(!proc.hasWaitingTime());
    QVERIFY(!proc.hasTurnaroundTime());
    QVERIFY(!proc.hasCompletionTime());

    // Verify default state is Ready
    QCOMPARE(proc.state, Models::ProcessState::Ready);
}

void TestProcessModel::testSetAndGetIdentities()
{
    Models::Process proc;
    proc.flightId = 42;
    proc.setQueueId(108);

    QCOMPARE(proc.flightId, 42);
    QCOMPARE(proc.processId(), 42);
    QCOMPARE(proc.id, 108);
    QCOMPARE(proc.queueId, 108);
    QVERIFY(proc.isPersisted());
    QVERIFY(!proc.isNew());
}

void TestProcessModel::testValidPriorityBoundaries()
{
    Models::Process proc;
    proc.flightId = 10;
    proc.arrivalTime = QDateTime::currentDateTime();
    proc.serviceTime = 60;

    // Priority 1 (highest priority)
    proc.basePriority = 1;
    proc.effectivePriority = 1;
    QString err;
    QVERIFY2(proc.validate(&err), qPrintable(err));

    // Priority 5 (lowest ordinary priority)
    proc.basePriority = 5;
    proc.effectivePriority = 5;
    err.clear();
    QVERIFY2(proc.validate(&err), qPrintable(err));
}

void TestProcessModel::testPriorityOutOfRangeRejection()
{
    Models::Process proc;
    proc.flightId = 10;
    proc.arrivalTime = QDateTime::currentDateTime();
    proc.serviceTime = 60;

    QString err;

    // Priority 0 is out of supported 1-5 range
    proc.basePriority = 0;
    proc.effectivePriority = 3;
    QVERIFY(!proc.validate(&err));
    QVERIFY(err.contains(QStringLiteral("Base priority must be between 1")));

    // Negative priority
    err.clear();
    proc.basePriority = -1;
    QVERIFY(!proc.validate(&err));

    // Priority 6
    err.clear();
    proc.basePriority = 6;
    QVERIFY(!proc.validate(&err));

    // Priority 10
    err.clear();
    proc.basePriority = 10;
    QVERIFY(!proc.validate(&err));

    // Reset base priority to valid; test invalid effective priority
    proc.basePriority = 3;
    proc.effectivePriority = 0;
    err.clear();
    QVERIFY(!proc.validate(&err));
    QVERIFY(err.contains(QStringLiteral("Effective priority must be between 1")));

    proc.effectivePriority = 6;
    err.clear();
    QVERIFY(!proc.validate(&err));
}

void TestProcessModel::testServiceTimeValidation()
{
    Models::Process proc;
    proc.flightId = 10;
    proc.basePriority = 3;
    proc.effectivePriority = 3;
    proc.arrivalTime = QDateTime::currentDateTime();

    QString err;

    // Valid service times
    proc.serviceTime = 1;
    QVERIFY(proc.validate(&err));

    proc.serviceTime = 60;
    QVERIFY(proc.validate(&err));

    proc.serviceTime = 300;
    QVERIFY(proc.validate(&err));

    // Invalid service times: 0
    err.clear();
    proc.serviceTime = 0;
    QVERIFY(!proc.validate(&err));
    QVERIFY(err.contains(QStringLiteral("Service time (burst time) must be greater than zero")));

    // Negative service time
    err.clear();
    proc.serviceTime = -10;
    QVERIFY(!proc.validate(&err));

    // Overflow beyond SMALLINT UNSIGNED (65535)
    err.clear();
    proc.serviceTime = 70000;
    QVERIFY(!proc.validate(&err));
    QVERIFY(err.contains(QStringLiteral("Service time cannot exceed 65535")));
}

void TestProcessModel::testBaseAndEffectivePrioritySeparation()
{
    Models::Process proc;
    proc.flightId = 5;
    proc.basePriority = 4;
    proc.effectivePriority = 4;

    // Mutating effective priority (as aging engine will do in Phase 2)
    proc.effectivePriority = 1;

    // Base priority must remain completely unaltered
    QCOMPARE(proc.basePriority, 4);
    QCOMPARE(proc.effectivePriority, 1);

    QString err;
    QVERIFY(proc.validate(&err));
}

void TestProcessModel::testProcessStateRepresentationAndMapping()
{
    // C++ enum to string representation
    QCOMPARE(Models::Process::stateToString(Models::ProcessState::New), QStringLiteral("New"));
    QCOMPARE(Models::Process::stateToString(Models::ProcessState::Ready), QStringLiteral("Ready"));
    QCOMPARE(Models::Process::stateToString(Models::ProcessState::Running), QStringLiteral("Running"));
    QCOMPARE(Models::Process::stateToString(Models::ProcessState::Blocked), QStringLiteral("Blocked"));
    QCOMPARE(Models::Process::stateToString(Models::ProcessState::Completed), QStringLiteral("Completed"));

    // DB column mapping ('New' maps to 'Ready' for the database queue table)
    QCOMPARE(Models::Process::stateToDbString(Models::ProcessState::New), QStringLiteral("Ready"));
    QCOMPARE(Models::Process::stateToDbString(Models::ProcessState::Ready), QStringLiteral("Ready"));
    QCOMPARE(Models::Process::stateToDbString(Models::ProcessState::Running), QStringLiteral("Running"));
    QCOMPARE(Models::Process::stateToDbString(Models::ProcessState::Blocked), QStringLiteral("Blocked"));
    QCOMPARE(Models::Process::stateToDbString(Models::ProcessState::Completed), QStringLiteral("Completed"));

    // String to C++ enum parsing
    bool ok = false;
    QCOMPARE(Models::Process::stateFromString(QStringLiteral("New"), &ok), Models::ProcessState::New);
    QVERIFY(ok);

    QCOMPARE(Models::Process::stateFromString(QStringLiteral("Ready"), &ok), Models::ProcessState::Ready);
    QVERIFY(ok);

    QCOMPARE(Models::Process::stateFromString(QStringLiteral("running"), &ok), Models::ProcessState::Running);
    QVERIFY(ok);

    QCOMPARE(Models::Process::stateFromString(QStringLiteral("BLOCKED"), &ok), Models::ProcessState::Blocked);
    QVERIFY(ok);

    QCOMPARE(Models::Process::stateFromString(QStringLiteral("Completed"), &ok), Models::ProcessState::Completed);
    QVERIFY(ok);

    // Invalid state fallback
    QCOMPARE(Models::Process::stateFromString(QStringLiteral("UnknownState"), &ok), Models::ProcessState::Ready);
    QVERIFY(!ok);
}

void TestProcessModel::testFlightToProcessMapping()
{
    Models::Flight f;
    f.id = 77;
    f.flightNumber = QStringLiteral("6E-304");
    f.airline = QStringLiteral("IndiGo");
    f.priority = 2;
    f.operationalStatus = QStringLiteral("Scheduled");

    // Map using default service time (60 seconds)
    Models::Process proc1 = Models::Process::fromFlight(f);
    QCOMPARE(proc1.flightId, 77);
    QCOMPARE(proc1.processId(), 77);
    QCOMPARE(proc1.basePriority, 2);
    QCOMPARE(proc1.effectivePriority, 2);
    QCOMPARE(proc1.serviceTime, 60);
    QCOMPARE(proc1.burstTime(), 60);
    QCOMPARE(proc1.state, Models::ProcessState::Ready);
    QCOMPARE(proc1.flightNumber, QStringLiteral("6E-304"));
    QCOMPARE(proc1.airline, QStringLiteral("IndiGo"));
    QVERIFY(proc1.isNew());
    QVERIFY(proc1.arrivalTime.isValid());
    QVERIFY(!proc1.hasWaitingTime());
    QVERIFY(!proc1.hasTurnaroundTime());
    QVERIFY(!proc1.hasCompletionTime());

    // Map using user-specified service time (90 seconds)
    Models::Process proc2 = Models::Process::fromFlight(f, 90);
    QCOMPARE(proc2.serviceTime, 90);
    QCOMPARE(proc2.burstTime(), 90);

    QString err;
    QVERIFY(proc1.validate(&err));
    QVERIFY(proc2.validate(&err));
}

void TestProcessModel::testFlightEligibilityRules()
{
    Models::Flight f;
    f.id = 50;
    f.flightNumber = QStringLiteral("AI-101");
    f.priority = 3;

    QString reason;

    // Eligible statuses
    f.operationalStatus = QStringLiteral("Scheduled");
    QVERIFY(Models::Process::isFlightEligible(f, &reason));

    f.operationalStatus = QStringLiteral("Taxiing");
    QVERIFY(Models::Process::isFlightEligible(f, &reason));

    f.operationalStatus = QStringLiteral("Cleared");
    QVERIFY(Models::Process::isFlightEligible(f, &reason));

    // Terminal/cancelled statuses must be rejected
    f.operationalStatus = QStringLiteral("Landed");
    reason.clear();
    QVERIFY(!Models::Process::isFlightEligible(f, &reason));
    QVERIFY(reason.contains(QStringLiteral("already landed")));

    f.operationalStatus = QStringLiteral("Departed");
    reason.clear();
    QVERIFY(!Models::Process::isFlightEligible(f, &reason));
    QVERIFY(reason.contains(QStringLiteral("already departed")));

    f.operationalStatus = QStringLiteral("Cancelled");
    reason.clear();
    QVERIFY(!Models::Process::isFlightEligible(f, &reason));
    QVERIFY(reason.contains(QStringLiteral("is cancelled")));

    // Unsaved flight (id <= 0) must be rejected
    f.id = 0;
    f.operationalStatus = QStringLiteral("Scheduled");
    reason.clear();
    QVERIFY(!Models::Process::isFlightEligible(f, &reason));
    QVERIFY(reason.contains(QStringLiteral("no persistent database ID")));
}

void TestProcessModel::testMetricsRemainUncalculated()
{
    Models::Process proc;
    proc.flightId = 15;
    proc.basePriority = 3;
    proc.effectivePriority = 3;
    proc.serviceTime = 60;
    proc.arrivalTime = QDateTime::currentDateTime();

    // Verify initial metric values are uncalculated
    QCOMPARE(proc.waitingTime, -1);
    QCOMPARE(proc.turnaroundTime, -1);
    QVERIFY(!proc.completionTime.isValid());

    // Validating process with uncalculated metrics must succeed
    QString err;
    QVERIFY(proc.validate(&err));
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Service Boundary & Role-Based Access Control
// ─────────────────────────────────────────────────────────────────────────────

void TestProcessModel::testUnauthenticatedCannotEnqueueFlight()
{
    m_service.logout();
    QVERIFY(!m_service.isAuthenticated());

    QString err;
    bool success = m_service.createProcessForFlight(1, 60, nullptr, &err);
    QVERIFY(!success);
    QVERIFY(err.contains(QStringLiteral("Authentication required")));
}

void TestProcessModel::testOperatorRoleDeniedProcessCreation()
{
    // Airport Operator role should not be authorized to manage scheduling queue
    m_service.setTestSession(Session(3, QStringLiteral("op_user"), QStringLiteral("Operator"), QStringLiteral("Airport Operator")));
    QVERIFY(m_service.isAuthenticated());

    QString err;
    bool success = m_service.createProcessForFlight(1, 60, nullptr, &err);
    QVERIFY(!success);
    QVERIFY(err.contains(QStringLiteral("Access denied")));
}

void TestProcessModel::testATCRolePermittedProcessCreation()
{
    m_service.setTestSession(Session(2, QStringLiteral("atc_user"), QStringLiteral("ATC User"), QStringLiteral("ATC")));
    QVERIFY(m_service.currentSession().hasPermission(QStringLiteral("action:ManageScheduling")));
}

void TestProcessModel::testAdminRolePermittedProcessCreation()
{
    m_service.setTestSession(Session(1, QStringLiteral("admin_user"), QStringLiteral("Admin User"), QStringLiteral("Admin")));
    QVERIFY(m_service.currentSession().hasPermission(QStringLiteral("action:ManageScheduling")));
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Live MySQL Persistence & Duplicate Prevention
// ─────────────────────────────────────────────────────────────────────────────

void TestProcessModel::testLiveProcessCreationAndRetrieval()
{
    if (!m_dbConnected || m_testFlightId1 <= 0) {
        QSKIP("Live MySQL database not available; skipping live persistence test.");
    }

    m_service.setTestSession(Session(2, QStringLiteral("atc_live"), QStringLiteral("ATC Live"), QStringLiteral("ATC")));

    // Verify flight does not yet have a process
    QString err;
    QVERIFY(!m_service.hasProcessForFlight(m_testFlightId1, &err));

    // Create process with custom service time of 75 seconds
    Models::Process createdProc;
    bool created = m_service.createProcessForFlight(m_testFlightId1, 75, &createdProc, &err);
    QVERIFY2(created, qPrintable(err));
    QVERIFY(createdProc.id > 0);
    QVERIFY(createdProc.isPersisted());
    QCOMPARE(createdProc.flightId, m_testFlightId1);
    QCOMPARE(createdProc.serviceTime, 75);
    QCOMPARE(createdProc.burstTime(), 75);
    QCOMPARE(createdProc.basePriority, 2);
    QCOMPARE(createdProc.effectivePriority, 2);
    QCOMPARE(createdProc.state, Models::ProcessState::Ready);

    // Verify hasProcessForFlight returns true
    QVERIFY(m_service.hasProcessForFlight(m_testFlightId1));

    // Retrieve by flight ID
    Models::Process retrievedByFlight;
    bool gotByFlight = m_service.getProcessByFlightId(m_testFlightId1, retrievedByFlight, &err);
    QVERIFY2(gotByFlight, qPrintable(err));
    QCOMPARE(retrievedByFlight.id, createdProc.id);
    QCOMPARE(retrievedByFlight.flightId, m_testFlightId1);
    QCOMPARE(retrievedByFlight.serviceTime, 75);
    QCOMPARE(retrievedByFlight.basePriority, 2);
    QCOMPARE(retrievedByFlight.state, Models::ProcessState::Ready);
    QCOMPARE(retrievedByFlight.waitingTime, -1);
    QCOMPARE(retrievedByFlight.turnaroundTime, -1);
    QVERIFY(!retrievedByFlight.hasCompletionTime());

    // Retrieve by queue ID
    Models::Process retrievedById;
    bool gotById = m_service.getProcessById(createdProc.id, retrievedById, &err);
    QVERIFY2(gotById, qPrintable(err));
    QCOMPARE(retrievedById.id, createdProc.id);
    QCOMPARE(retrievedById.flightId, m_testFlightId1);

    // Verify present in getAllProcesses
    QList<Models::Process> all = m_service.getAllProcesses(&err);
    QVERIFY(!all.isEmpty());
    bool foundInList = false;
    for (const auto &p : all) {
        if (p.id == createdProc.id) {
            foundInList = true;
            break;
        }
    }
    QVERIFY(foundInList);
}

void TestProcessModel::testLiveDuplicateProcessRejection()
{
    if (!m_dbConnected || m_testFlightId1 <= 0) {
        QSKIP("Live MySQL database not available; skipping duplicate test.");
    }

    m_service.setTestSession(Session(2, QStringLiteral("atc_live"), QStringLiteral("ATC Live"), QStringLiteral("ATC")));

    // Flight m_testFlightId1 already has an active scheduling record
    QString err;
    Models::Process duplicateProc;
    bool success = m_service.createProcessForFlight(m_testFlightId1, 60, &duplicateProc, &err);

    // Must be rejected predictably
    QVERIFY(!success);
    QVERIFY2(err.contains(QStringLiteral("already has an active scheduling record")), qPrintable(err));
}

void TestProcessModel::testLiveIneligibleFlightProcessCreationRejection()
{
    if (!m_dbConnected || m_testFlightId2 <= 0) {
        QSKIP("Live MySQL database not available; skipping ineligible flight test.");
    }

    m_service.setTestSession(Session(2, QStringLiteral("atc_live"), QStringLiteral("ATC Live"), QStringLiteral("ATC")));

    // m_testFlightId2 is in 'Landed' status (ineligible)
    QString err;
    Models::Process proc;
    bool success = m_service.createProcessForFlight(m_testFlightId2, 60, &proc, &err);

    // Must be rejected
    QVERIFY(!success);
    QVERIFY2(err.contains(QStringLiteral("already landed"), Qt::CaseInsensitive), qPrintable(err));
}

QTEST_MAIN(TestProcessModel)
#include "TestProcessModel.moc"
