#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>

#include "models/Flight.h"
#include "models/Session.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"

class TestFlightManagement : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ── 1. Model & Validation Tests ──────────────────────────────────────────
    void testValidFlightValidation();
    void testEmptyFlightNumberRejection();
    void testExcessiveFlightNumberLength();
    void testEmptyAirlineRejection();
    void testEmptySourceAndDestinationRejection();
    void testIdenticalSourceAndDestinationRejection();
    void testPriorityRangeValidation();
    void testInvalidFlightTypeRejection();
    void testInvalidOperationalStatusRejection();
    void testInvalidScheduledTimeRejection();
    void testNegativeAircraftIdRejection();
    void testOptionalAircraftIdAllowed();

    // ── 2. Role-Based Access Control at Service Boundary ─────────────────────
    void testUnauthenticatedCannotCreateOrModify();
    void testAdminRoleDeniedFlightModifications();
    void testOperatorRoleDeniedFlightModifications();
    void testATCRolePermittedFlightModifications();

    // ── 3. Live MySQL CRUD & Referential Integrity Tests ─────────────────────
    void testLiveGetAllFlights();
    void testLiveFlightSearchAndStatusFilter();
    void testLiveCreateReadUpdateDeleteCycle();
    void testLiveReferentialIntegritySafeDelete();
    void testLiveAvailableAircraftOptions();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;
};

void TestFlightManagement::initTestCase()
{
    // Initialize application service and database connectivity
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();
    if (!m_dbConnected) {
        qWarning() << "[TestFlightManagement] Live database connection unavailable; testing offline functionality only.";
    }
}

void TestFlightManagement::cleanupTestCase()
{
    // Clean up any remaining test records if DB was used
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'TEST-%';"));
        }
    }
    m_service.shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Model & Validation Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestFlightManagement::testValidFlightValidation()
{
    Models::Flight f;
    f.flightNumber = QStringLiteral("AI-202");
    f.airline = QStringLiteral("Air India");
    f.source = QStringLiteral("DEL");
    f.destination = QStringLiteral("BOM");
    f.flightType = QStringLiteral("Arrival");
    f.scheduledTime = QDateTime::currentDateTime();
    f.priority = 3;
    f.isEmergency = false;
    f.operationalStatus = QStringLiteral("Scheduled");
    f.aircraftId = 0;

    QString error;
    QVERIFY2(f.validate(&error), qPrintable(error));
    QVERIFY(error.isEmpty());
}

void TestFlightManagement::testEmptyFlightNumberRejection()
{
    Models::Flight f;
    f.flightNumber = QStringLiteral("   ");
    f.airline = QStringLiteral("Air India");
    f.source = QStringLiteral("DEL");
    f.destination = QStringLiteral("BOM");
    f.scheduledTime = QDateTime::currentDateTime();

    QString error;
    QVERIFY(!f.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Flight number is required"), Qt::CaseInsensitive));
}

void TestFlightManagement::testExcessiveFlightNumberLength()
{
    Models::Flight f;
    f.flightNumber = QStringLiteral("ABCDEFGHIJKL"); // 12 chars > 10
    f.airline = QStringLiteral("Air India");
    f.source = QStringLiteral("DEL");
    f.destination = QStringLiteral("BOM");
    f.scheduledTime = QDateTime::currentDateTime();

    QString error;
    QVERIFY(!f.validate(&error));
    QVERIFY(error.contains(QStringLiteral("cannot exceed 10 characters"), Qt::CaseInsensitive));
}

void TestFlightManagement::testEmptyAirlineRejection()
{
    Models::Flight f;
    f.flightNumber = QStringLiteral("AI-101");
    f.airline = QStringLiteral("");
    f.source = QStringLiteral("DEL");
    f.destination = QStringLiteral("BOM");
    f.scheduledTime = QDateTime::currentDateTime();

    QString error;
    QVERIFY(!f.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Airline name is required"), Qt::CaseInsensitive));
}

void TestFlightManagement::testEmptySourceAndDestinationRejection()
{
    Models::Flight f;
    f.flightNumber = QStringLiteral("AI-101");
    f.airline = QStringLiteral("Air India");
    f.source = QStringLiteral("");
    f.destination = QStringLiteral("BOM");
    f.scheduledTime = QDateTime::currentDateTime();

    QString error;
    QVERIFY(!f.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Source airport is required"), Qt::CaseInsensitive));

    f.source = QStringLiteral("DEL");
    f.destination = QStringLiteral("   ");
    QVERIFY(!f.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Destination airport is required"), Qt::CaseInsensitive));
}

void TestFlightManagement::testIdenticalSourceAndDestinationRejection()
{
    Models::Flight f;
    f.flightNumber = QStringLiteral("6E-303");
    f.airline = QStringLiteral("IndiGo");
    f.source = QStringLiteral("BOM");
    f.destination = QStringLiteral("bom"); // Case-insensitive duplicate
    f.scheduledTime = QDateTime::currentDateTime();

    QString error;
    QVERIFY(!f.validate(&error));
    QVERIFY(error.contains(QStringLiteral("cannot be identical"), Qt::CaseInsensitive));
}

void TestFlightManagement::testPriorityRangeValidation()
{
    Models::Flight f;
    f.flightNumber = QStringLiteral("6E-303");
    f.airline = QStringLiteral("IndiGo");
    f.source = QStringLiteral("BOM");
    f.destination = QStringLiteral("BLR");
    f.scheduledTime = QDateTime::currentDateTime();

    f.priority = 0; // below 1
    QString error;
    QVERIFY(!f.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Priority must be between 1"), Qt::CaseInsensitive));

    f.priority = 6; // above 5
    QVERIFY(!f.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Priority must be between 1"), Qt::CaseInsensitive));

    f.priority = 1;
    QVERIFY(f.validate(&error));

    f.priority = 5;
    QVERIFY(f.validate(&error));
}

void TestFlightManagement::testInvalidFlightTypeRejection()
{
    Models::Flight f;
    f.flightNumber = QStringLiteral("6E-303");
    f.airline = QStringLiteral("IndiGo");
    f.source = QStringLiteral("BOM");
    f.destination = QStringLiteral("BLR");
    f.scheduledTime = QDateTime::currentDateTime();
    f.flightType = QStringLiteral("Charter"); // Invalid, only Arrival or Departure permitted

    QString error;
    QVERIFY(!f.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Flight type must be 'Arrival' or 'Departure'"), Qt::CaseInsensitive));
}

void TestFlightManagement::testInvalidOperationalStatusRejection()
{
    Models::Flight f;
    f.flightNumber = QStringLiteral("6E-303");
    f.airline = QStringLiteral("IndiGo");
    f.source = QStringLiteral("BOM");
    f.destination = QStringLiteral("BLR");
    f.scheduledTime = QDateTime::currentDateTime();
    f.operationalStatus = QStringLiteral("InFlight"); // Invalid status

    QString error;
    QVERIFY(!f.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Invalid operational status"), Qt::CaseInsensitive));
}

void TestFlightManagement::testInvalidScheduledTimeRejection()
{
    Models::Flight f;
    f.flightNumber = QStringLiteral("6E-303");
    f.airline = QStringLiteral("IndiGo");
    f.source = QStringLiteral("BOM");
    f.destination = QStringLiteral("BLR");
    f.scheduledTime = QDateTime(); // invalid / null datetime

    QString error;
    QVERIFY(!f.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Scheduled time must be a valid date"), Qt::CaseInsensitive));
}

void TestFlightManagement::testNegativeAircraftIdRejection()
{
    Models::Flight f;
    f.flightNumber = QStringLiteral("6E-303");
    f.airline = QStringLiteral("IndiGo");
    f.source = QStringLiteral("BOM");
    f.destination = QStringLiteral("BLR");
    f.scheduledTime = QDateTime::currentDateTime();
    f.aircraftId = -5;

    QString error;
    QVERIFY(!f.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Aircraft ID cannot be negative"), Qt::CaseInsensitive));
}

void TestFlightManagement::testOptionalAircraftIdAllowed()
{
    Models::Flight f;
    f.flightNumber = QStringLiteral("6E-303");
    f.airline = QStringLiteral("IndiGo");
    f.source = QStringLiteral("BOM");
    f.destination = QStringLiteral("BLR");
    f.scheduledTime = QDateTime::currentDateTime();
    f.aircraftId = 0; // 0 denotes unassigned (NULL in DB)

    QString error;
    QVERIFY(f.validate(&error));
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Role-Based Access Control at Service Boundary
// ─────────────────────────────────────────────────────────────────────────────

void TestFlightManagement::testUnauthenticatedCannotCreateOrModify()
{
    m_service.logout();
    QVERIFY(!m_service.isAuthenticated());

    Models::Flight f;
    f.flightNumber = QStringLiteral("TEST-UNAUTH");
    f.airline = QStringLiteral("Test Airlines");
    f.source = QStringLiteral("DEL");
    f.destination = QStringLiteral("BOM");
    f.scheduledTime = QDateTime::currentDateTime();

    QString error;
    QVERIFY(!m_service.createFlight(f, &error));
    QVERIFY(error.contains(QStringLiteral("Authentication required"), Qt::CaseInsensitive));

    f.id = 9999;
    QVERIFY(!m_service.updateFlight(f, &error));
    QVERIFY(error.contains(QStringLiteral("Authentication required"), Qt::CaseInsensitive));

    QVERIFY(!m_service.deleteFlight(9999, &error));
    QVERIFY(error.contains(QStringLiteral("Authentication required"), Qt::CaseInsensitive));
}

void TestFlightManagement::testAdminRoleDeniedFlightModifications()
{
    // Admin has page:Flights (view-only), but NOT action:ManageFlights
    Session adminSession(1, QStringLiteral("admin_demo"), QStringLiteral("Admin User"), QStringLiteral("Admin"));
    m_service.setTestSession(adminSession);
    QVERIFY(m_service.currentSession().isAdmin());
    QVERIFY(m_service.currentSession().hasPermission(QStringLiteral("page:Flights")));
    QVERIFY(!m_service.currentSession().hasPermission(QStringLiteral("action:ManageFlights")));

    Models::Flight f;
    f.flightNumber = QStringLiteral("TEST-ADM");
    f.airline = QStringLiteral("Test Airlines");
    f.source = QStringLiteral("DEL");
    f.destination = QStringLiteral("BOM");
    f.scheduledTime = QDateTime::currentDateTime();

    QString error;
    QVERIFY(!m_service.createFlight(f, &error));
    QVERIFY(error.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));

    f.id = 1;
    QVERIFY(!m_service.updateFlight(f, &error));
    QVERIFY(error.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));

    QVERIFY(!m_service.deleteFlight(1, &error));
    QVERIFY(error.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));
}

void TestFlightManagement::testOperatorRoleDeniedFlightModifications()
{
    // Airport Operator has page:Flights (view-only), but NOT action:ManageFlights
    Session opSession(3, QStringLiteral("operator_demo"), QStringLiteral("Operator User"), QStringLiteral("Airport Operator"));
    m_service.setTestSession(opSession);
    QVERIFY(m_service.currentSession().isOperator());
    QVERIFY(m_service.currentSession().hasPermission(QStringLiteral("page:Flights")));
    QVERIFY(!m_service.currentSession().hasPermission(QStringLiteral("action:ManageFlights")));

    Models::Flight f;
    f.flightNumber = QStringLiteral("TEST-OP");
    f.airline = QStringLiteral("Test Airlines");
    f.source = QStringLiteral("DEL");
    f.destination = QStringLiteral("BOM");
    f.scheduledTime = QDateTime::currentDateTime();

    QString error;
    QVERIFY(!m_service.createFlight(f, &error));
    QVERIFY(error.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));

    f.id = 1;
    QVERIFY(!m_service.updateFlight(f, &error));
    QVERIFY(error.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));

    QVERIFY(!m_service.deleteFlight(1, &error));
    QVERIFY(error.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));
}

void TestFlightManagement::testATCRolePermittedFlightModifications()
{
    // ATC role has action:ManageFlights
    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("Radar Controller"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);
    QVERIFY(m_service.currentSession().isATC());
    QVERIFY(m_service.currentSession().hasPermission(QStringLiteral("action:ManageFlights")));
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Live MySQL CRUD & Referential Integrity Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestFlightManagement::testLiveGetAllFlights()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping live query test.");
    }

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("Radar Controller"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    QList<Models::Flight> flights = m_service.getAllFlights(QString(), QString(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!flights.isEmpty());

    // Verify seed records are present and joined with aircraft data
    bool foundAI101 = false;
    for (const auto &f : flights) {
        if (f.flightNumber == QStringLiteral("AI-101")) {
            foundAI101 = true;
            QCOMPARE(f.airline, QStringLiteral("Air India"));
            QCOMPARE(f.isEmergency, true);
            QCOMPARE(f.priority, 1);
            QVERIFY(!f.aircraftRegistration.isEmpty());
            break;
        }
    }
    QVERIFY2(foundAI101, "Seed flight AI-101 should exist in database");
}

void TestFlightManagement::testLiveFlightSearchAndStatusFilter()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping search test.");
    }

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("Radar Controller"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    // Search by flight number
    QList<Models::Flight> searchResults = m_service.getAllFlights(QStringLiteral("AI-101"), QString(), &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(searchResults.size(), 1);
    QCOMPARE(searchResults.first().flightNumber, QStringLiteral("AI-101"));

    // Filter by status 'Scheduled'
    QList<Models::Flight> schedResults = m_service.getAllFlights(QString(), QStringLiteral("Scheduled"), &error);
    QVERIFY(error.isEmpty());
    for (const auto &f : schedResults) {
        QCOMPARE(f.operationalStatus, QStringLiteral("Scheduled"));
    }
}

void TestFlightManagement::testLiveCreateReadUpdateDeleteCycle()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping CRUD test.");
    }

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("Radar Controller"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // 1. Create a valid test flight
    Models::Flight testFlight;
    testFlight.flightNumber = QStringLiteral("TEST-701");
    testFlight.airline = QStringLiteral("Test Aero");
    testFlight.source = QStringLiteral("DEL");
    testFlight.destination = QStringLiteral("BOM");
    testFlight.flightType = QStringLiteral("Arrival");
    testFlight.scheduledTime = QDateTime(QDate(2026, 10, 20), QTime(14, 30, 0));
    testFlight.priority = 2;
    testFlight.isEmergency = false;
    testFlight.operationalStatus = QStringLiteral("Scheduled");
    testFlight.aircraftId = 0; // unassigned

    QString error;
    bool created = m_service.createFlight(testFlight, &error);
    QVERIFY2(created, qPrintable(error));

    // 2. Read back the created flight
    QList<Models::Flight> foundList = m_service.getAllFlights(QStringLiteral("TEST-701"), QString(), &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(foundList.size(), 1);

    Models::Flight createdFlight = foundList.first();
    QVERIFY(createdFlight.id > 0);
    QCOMPARE(createdFlight.flightNumber, QStringLiteral("TEST-701"));
    QCOMPARE(createdFlight.airline, QStringLiteral("Test Aero"));
    QCOMPARE(createdFlight.effectiveSource(), QStringLiteral("DEL"));
    QCOMPARE(createdFlight.destination, QStringLiteral("BOM"));
    QCOMPARE(createdFlight.priority, 2);
    QCOMPARE(createdFlight.isEmergency, false);
    QCOMPARE(createdFlight.operationalStatus, QStringLiteral("Scheduled"));

    // 3. Update the flight
    createdFlight.airline = QStringLiteral("Test Aero International");
    createdFlight.priority = 1;
    createdFlight.isEmergency = true;
    createdFlight.operationalStatus = QStringLiteral("Taxiing");

    bool updated = m_service.updateFlight(createdFlight, &error);
    QVERIFY2(updated, qPrintable(error));

    // Verify update persisted
    Models::Flight reRead;
    bool readOk = m_service.getFlightById(createdFlight.id, reRead, &error);
    QVERIFY2(readOk, qPrintable(error));
    QCOMPARE(reRead.airline, QStringLiteral("Test Aero International"));
    QCOMPARE(reRead.priority, 1);
    QCOMPARE(reRead.isEmergency, true);
    QCOMPARE(reRead.operationalStatus, QStringLiteral("Taxiing"));

    // 4. Delete the flight safely
    bool deleted = m_service.deleteFlight(createdFlight.id, &error);
    QVERIFY2(deleted, qPrintable(error));

    // Verify deletion persisted
    Models::Flight deletedCheck;
    bool stillExists = m_service.getFlightById(createdFlight.id, deletedCheck, &error);
    QVERIFY(!stillExists);
}

void TestFlightManagement::testLiveReferentialIntegritySafeDelete()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping referential integrity test.");
    }

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("Radar Controller"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // AI-101 has a runway assignment in seed.sql (fk_ra_flight_id ON DELETE RESTRICT)
    QString error;
    QList<Models::Flight> list = m_service.getAllFlights(QStringLiteral("AI-101"), QString(), &error);
    QVERIFY(!list.isEmpty());

    int ai101Id = list.first().id;

    // canDeleteFlight should return false with referential integrity explanation
    QString reason;
    bool canDel = m_service.canDeleteFlight(ai101Id, &reason);
    QVERIFY(!canDel);
    QVERIFY(reason.contains(QStringLiteral("runway assignment"), Qt::CaseInsensitive));

    // deleteFlight should be blocked safely and report the error without crashing
    bool delSuccess = m_service.deleteFlight(ai101Id, &error);
    QVERIFY(!delSuccess);
    QVERIFY(error.contains(QStringLiteral("runway assignment"), Qt::CaseInsensitive));

    // Verify flight record was NOT deleted
    Models::Flight stillThere;
    QVERIFY(m_service.getFlightById(ai101Id, stillThere, &error));
    QCOMPARE(stillThere.flightNumber, QStringLiteral("AI-101"));
}

void TestFlightManagement::testLiveAvailableAircraftOptions()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping aircraft options test.");
    }

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("Radar Controller"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    QString error;
    QList<QPair<int, QString>> options = m_service.getAvailableAircraft(&error);
    QVERIFY(error.isEmpty());
    QVERIFY(options.size() > 1); // unassigned + registered aircraft

    // Check first is unassigned placeholder
    QCOMPARE(options.first().first, 0);

    // Check seed aircraft exists in options
    bool foundVTAT1 = false;
    for (const auto &opt : options) {
        if (opt.second.contains(QStringLiteral("VT-AT1"))) {
            foundVTAT1 = true;
            break;
        }
    }
    QVERIFY(foundVTAT1);
}

QTEST_MAIN(TestFlightManagement)
#include "TestFlightManagement.moc"
