#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>

#include "models/Aircraft.h"
#include "models/Flight.h"
#include "models/Session.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"

class TestAircraftManagement : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ── 1. Model & Validation Tests ──────────────────────────────────────────
    void testValidAircraftValidation();
    void testEmptyRegistrationRejection();
    void testExcessiveRegistrationLength();
    void testEmptyAircraftTypeRejection();
    void testExcessiveAircraftTypeLength();
    void testZeroAndNegativeCapacityRejection();
    void testValidCapacityAccepted();
    void testInvalidStatusRejection();
    void testValidStatusesAccepted();
    void testTrimmingAndNormalization();

    // ── 2. Role-Based Access Control at Service Boundary ─────────────────────
    void testUnauthenticatedCannotAccessOrModify();
    void testATCRoleViewOnlyDeniedModifications();
    void testOperatorRoleStatusUpdateOnlyDeniedRegistration();
    void testOperatorRoleDeniedDeletion();
    void testAdminRolePermittedFullAircraftAdministration();

    // ── 3. Live MySQL CRUD & Safe Deletion Tests ──────────────────────────────
    void testLiveGetAllAircraft();
    void testLiveAircraftSearchAndStatusFilter();
    void testLiveGetAircraftById();
    void testLiveCreateReadUpdateDeleteCycle();
    void testLiveRejectDuplicateRegistration();
    void testLiveOperatorStatusUpdateOnly();
    void testLiveReferentialIntegrityPreventsReferencedDeletion();
    void testLiveAvailableAircraftForFlightAssignment();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;
};

void TestAircraftManagement::initTestCase()
{
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();
    if (!m_dbConnected) {
        qWarning() << "[TestAircraftManagement] Live database connection unavailable; testing offline functionality only.";
    }
}

void TestAircraftManagement::cleanupTestCase()
{
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            // Clean up test flights and test aircraft only
            q.exec(QStringLiteral("DELETE FROM flights WHERE flight_number LIKE 'TEST-AC%';"));
            q.exec(QStringLiteral("DELETE FROM aircraft WHERE registration_number LIKE 'VT-TEST%';"));
        }
    }
    m_service.shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Model & Validation Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestAircraftManagement::testValidAircraftValidation()
{
    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("VT-AX1");
    ac.aircraftType = QStringLiteral("Boeing 737-800");
    ac.capacity = 189;
    ac.status = QStringLiteral("Active");

    QString error;
    QVERIFY2(ac.validate(&error), qPrintable(error));
    QVERIFY(error.isEmpty());
}

void TestAircraftManagement::testEmptyRegistrationRejection()
{
    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("   ");
    ac.aircraftType = QStringLiteral("Airbus A320");
    ac.capacity = 180;
    ac.status = QStringLiteral("Active");

    QString error;
    QVERIFY(!ac.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Registration number is required"), Qt::CaseInsensitive));
}

void TestAircraftManagement::testExcessiveRegistrationLength()
{
    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("VT-123456789012345678"); // 21 chars > 20
    ac.aircraftType = QStringLiteral("Airbus A320");
    ac.capacity = 180;
    ac.status = QStringLiteral("Active");

    QString error;
    QVERIFY(!ac.validate(&error));
    QVERIFY(error.contains(QStringLiteral("cannot exceed 20 characters"), Qt::CaseInsensitive));
}

void TestAircraftManagement::testEmptyAircraftTypeRejection()
{
    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("VT-AX2");
    ac.aircraftType = QStringLiteral("  ");
    ac.capacity = 180;
    ac.status = QStringLiteral("Active");

    QString error;
    QVERIFY(!ac.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Aircraft type / model is required"), Qt::CaseInsensitive));
}

void TestAircraftManagement::testExcessiveAircraftTypeLength()
{
    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("VT-AX2");
    ac.aircraftType = QStringLiteral("This is an extremely long aircraft type name that exceeds the maximum fifty characters limit!");
    ac.capacity = 180;
    ac.status = QStringLiteral("Active");

    QString error;
    QVERIFY(!ac.validate(&error));
    QVERIFY(error.contains(QStringLiteral("cannot exceed 50 characters"), Qt::CaseInsensitive));
}

void TestAircraftManagement::testZeroAndNegativeCapacityRejection()
{
    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("VT-AX3");
    ac.aircraftType = QStringLiteral("Boeing 777");
    ac.status = QStringLiteral("Active");

    ac.capacity = 0;
    QString error;
    QVERIFY(!ac.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Capacity must be greater than zero"), Qt::CaseInsensitive));

    ac.capacity = -10;
    QVERIFY(!ac.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Capacity must be greater than zero"), Qt::CaseInsensitive));
}

void TestAircraftManagement::testValidCapacityAccepted()
{
    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("VT-AX3");
    ac.aircraftType = QStringLiteral("Boeing 777-300ER");
    ac.status = QStringLiteral("Active");

    ac.capacity = 1;
    QString error;
    QVERIFY(ac.validate(&error));

    ac.capacity = 396;
    QVERIFY(ac.validate(&error));
}

void TestAircraftManagement::testInvalidStatusRejection()
{
    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("VT-AX4");
    ac.aircraftType = QStringLiteral("Boeing 787");
    ac.capacity = 250;
    ac.status = QStringLiteral("Flying"); // Not in ENUM('Active', 'Maintenance', 'Retired')

    QString error;
    QVERIFY(!ac.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Invalid operational status"), Qt::CaseInsensitive));
}

void TestAircraftManagement::testValidStatusesAccepted()
{
    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("VT-AX5");
    ac.aircraftType = QStringLiteral("Bombardier Q400");
    ac.capacity = 78;

    QString error;
    ac.status = QStringLiteral("Active");
    QVERIFY(ac.validate(&error));

    ac.status = QStringLiteral("Maintenance");
    QVERIFY(ac.validate(&error));

    ac.status = QStringLiteral("Retired");
    QVERIFY(ac.validate(&error));
}

void TestAircraftManagement::testTrimmingAndNormalization()
{
    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("  vt-test1  ");
    ac.aircraftType = QStringLiteral("  Airbus A321  ");
    ac.capacity = 220;
    ac.status = QStringLiteral("Active");

    QString error;
    QVERIFY(ac.validate(&error));
    QCOMPARE(ac.registrationNumber, QStringLiteral("VT-TEST1"));
    QCOMPARE(ac.aircraftType, QStringLiteral("Airbus A321"));
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Role-Based Access Control at Service Boundary
// ─────────────────────────────────────────────────────────────────────────────

void TestAircraftManagement::testUnauthenticatedCannotAccessOrModify()
{
    m_service.logout();
    QVERIFY(!m_service.isAuthenticated());

    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("VT-TEST-UNAUTH");
    ac.aircraftType = QStringLiteral("Boeing 737");
    ac.capacity = 180;
    ac.status = QStringLiteral("Active");

    QString error;
    QVERIFY(!m_service.createAircraft(ac, &error));
    QVERIFY(error.contains(QStringLiteral("Authentication required"), Qt::CaseInsensitive));

    ac.id = 1;
    QVERIFY(!m_service.updateAircraft(ac, &error));
    QVERIFY(error.contains(QStringLiteral("Authentication required"), Qt::CaseInsensitive));

    QVERIFY(!m_service.updateAircraftStatus(1, QStringLiteral("Maintenance"), &error));
    QVERIFY(error.contains(QStringLiteral("Authentication required"), Qt::CaseInsensitive));

    QVERIFY(!m_service.deleteAircraft(1, &error));
    QVERIFY(error.contains(QStringLiteral("Authentication required"), Qt::CaseInsensitive));
}

void TestAircraftManagement::testATCRoleViewOnlyDeniedModifications()
{
    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("Radar Controller"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // ATC has page:Aircraft permission to view fleet during flight ops
    QVERIFY(m_service.currentSession().hasPermission(QStringLiteral("page:Aircraft")));
    // But ATC is denied aircraft administration and management
    QVERIFY(!m_service.currentSession().hasPermission(QStringLiteral("action:AdministerAircraft")));
    QVERIFY(!m_service.currentSession().hasPermission(QStringLiteral("action:ManageAircraft")));

    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("VT-TEST-ATC");
    ac.aircraftType = QStringLiteral("Airbus A320");
    ac.capacity = 180;
    ac.status = QStringLiteral("Active");

    QString error;
    QVERIFY(!m_service.createAircraft(ac, &error));
    QVERIFY(error.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));

    ac.id = 1;
    QVERIFY(!m_service.updateAircraft(ac, &error));
    QVERIFY(error.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));

    QVERIFY(!m_service.updateAircraftStatus(1, QStringLiteral("Maintenance"), &error));
    QVERIFY(error.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));

    QVERIFY(!m_service.deleteAircraft(1, &error));
    QVERIFY(error.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive));
}

void TestAircraftManagement::testOperatorRoleStatusUpdateOnlyDeniedRegistration()
{
    Session opSession(3, QStringLiteral("operator_demo"), QStringLiteral("Operator User"), QStringLiteral("Airport Operator"));
    m_service.setTestSession(opSession);

    // Operator has page:Aircraft and action:ManageAircraft (for status updates)
    QVERIFY(m_service.currentSession().hasPermission(QStringLiteral("page:Aircraft")));
    QVERIFY(m_service.currentSession().hasPermission(QStringLiteral("action:ManageAircraft")));
    // Operator is NOT an Admin: denied action:AdministerAircraft (registration & deletion)
    QVERIFY(!m_service.currentSession().hasPermission(QStringLiteral("action:AdministerAircraft")));

    Models::Aircraft ac;
    ac.registrationNumber = QStringLiteral("VT-TEST-OP");
    ac.aircraftType = QStringLiteral("Airbus A350");
    ac.capacity = 300;
    ac.status = QStringLiteral("Active");

    QString error;
    // Registration must be denied to Operator
    QVERIFY(!m_service.createAircraft(ac, &error));
    QVERIFY(error.contains(QStringLiteral("Only Administrators can register new aircraft"), Qt::CaseInsensitive));
}

void TestAircraftManagement::testOperatorRoleDeniedDeletion()
{
    Session opSession(3, QStringLiteral("operator_demo"), QStringLiteral("Operator User"), QStringLiteral("Airport Operator"));
    m_service.setTestSession(opSession);

    QString error;
    QVERIFY(!m_service.deleteAircraft(9999, &error));
    QVERIFY(error.contains(QStringLiteral("Only Administrators can delete aircraft"), Qt::CaseInsensitive));
}

void TestAircraftManagement::testAdminRolePermittedFullAircraftAdministration()
{
    Session adminSession(1, QStringLiteral("admin_demo"), QStringLiteral("Admin User"), QStringLiteral("Admin"));
    m_service.setTestSession(adminSession);

    QVERIFY(m_service.currentSession().hasPermission(QStringLiteral("page:Aircraft")));
    QVERIFY(m_service.currentSession().hasPermission(QStringLiteral("action:ManageAircraft")));
    QVERIFY(m_service.currentSession().hasPermission(QStringLiteral("action:AdministerAircraft")));
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Live MySQL CRUD & Safe Deletion Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestAircraftManagement::testLiveGetAllAircraft()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping live query test.");
    }

    Session adminSession(1, QStringLiteral("admin_demo"), QStringLiteral("Admin User"), QStringLiteral("Admin"));
    m_service.setTestSession(adminSession);

    QString error;
    QList<Models::Aircraft> fleet = m_service.getAllAircraft(QString(), QString(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!fleet.isEmpty());

    // Verify seed aircraft VT-AT1 is present with flights count
    bool foundVTAT1 = false;
    for (const auto &ac : fleet) {
        if (ac.registrationNumber == QStringLiteral("VT-AT1")) {
            foundVTAT1 = true;
            QCOMPARE(ac.aircraftType, QStringLiteral("Boeing 737-800"));
            QCOMPARE(ac.capacity, 189);
            QCOMPARE(ac.status, QStringLiteral("Active"));
            QVERIFY(ac.assignedFlightsCount >= 1);
            break;
        }
    }
    QVERIFY2(foundVTAT1, "Seed aircraft VT-AT1 should exist in database");
}

void TestAircraftManagement::testLiveAircraftSearchAndStatusFilter()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping search test.");
    }

    Session adminSession(1, QStringLiteral("admin_demo"), QStringLiteral("Admin User"), QStringLiteral("Admin"));
    m_service.setTestSession(adminSession);

    QString error;
    // Search by registration
    QList<Models::Aircraft> res1 = m_service.getAllAircraft(QStringLiteral("VT-AT1"), QString(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(res1.size(), 1);
    QCOMPARE(res1[0].registrationNumber, QStringLiteral("VT-AT1"));

    // Search by aircraft type
    QList<Models::Aircraft> res2 = m_service.getAllAircraft(QStringLiteral("Airbus"), QString(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    for (const auto &ac : res2) {
        QVERIFY(ac.aircraftType.contains(QStringLiteral("Airbus"), Qt::CaseInsensitive));
    }

    // Filter by status Active
    QList<Models::Aircraft> res3 = m_service.getAllAircraft(QString(), QStringLiteral("Active"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    for (const auto &ac : res3) {
        QCOMPARE(ac.status, QStringLiteral("Active"));
    }
}

void TestAircraftManagement::testLiveGetAircraftById()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping getById test.");
    }

    Session adminSession(1, QStringLiteral("admin_demo"), QStringLiteral("Admin User"), QStringLiteral("Admin"));
    m_service.setTestSession(adminSession);

    QString error;
    Models::Aircraft ac;
    bool found = m_service.getAircraftById(1, ac, &error);
    QVERIFY2(found, qPrintable(error));
    QCOMPARE(ac.id, 1);
    QCOMPARE(ac.registrationNumber, QStringLiteral("VT-AT1"));
}

void TestAircraftManagement::testLiveCreateReadUpdateDeleteCycle()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping CRUD cycle test.");
    }

    Session adminSession(1, QStringLiteral("admin_demo"), QStringLiteral("Admin User"), QStringLiteral("Admin"));
    m_service.setTestSession(adminSession);

    // 1. Create test aircraft
    Models::Aircraft newAc;
    newAc.registrationNumber = QStringLiteral("VT-TEST100");
    newAc.aircraftType = QStringLiteral("Embraer E195");
    newAc.capacity = 124;
    newAc.status = QStringLiteral("Active");

    QString createErr;
    bool created = m_service.createAircraft(newAc, &createErr);
    QVERIFY2(created, qPrintable(createErr));
    QVERIFY(newAc.id > 0);

    // 2. Read back
    Models::Aircraft fetchedAc;
    QString readErr;
    bool fetched = m_service.getAircraftById(newAc.id, fetchedAc, &readErr);
    QVERIFY2(fetched, qPrintable(readErr));
    QCOMPARE(fetchedAc.registrationNumber, QStringLiteral("VT-TEST100"));
    QCOMPARE(fetchedAc.aircraftType, QStringLiteral("Embraer E195"));
    QCOMPARE(fetchedAc.capacity, 124);
    QCOMPARE(fetchedAc.status, QStringLiteral("Active"));

    // 3. Update full aircraft as Admin
    fetchedAc.aircraftType = QStringLiteral("Embraer E195-E2");
    fetchedAc.capacity = 132;
    fetchedAc.status = QStringLiteral("Maintenance");

    QString updateErr;
    bool updated = m_service.updateAircraft(fetchedAc, &updateErr);
    QVERIFY2(updated, qPrintable(updateErr));

    // Verify update persisted
    Models::Aircraft updatedAc;
    m_service.getAircraftById(newAc.id, updatedAc);
    QCOMPARE(updatedAc.aircraftType, QStringLiteral("Embraer E195-E2"));
    QCOMPARE(updatedAc.capacity, 132);
    QCOMPARE(updatedAc.status, QStringLiteral("Maintenance"));

    // 4. Delete unreferenced aircraft as Admin
    QString deleteErr;
    bool deleted = m_service.deleteAircraft(newAc.id, &deleteErr);
    QVERIFY2(deleted, qPrintable(deleteErr));

    // Verify deletion
    Models::Aircraft deletedAc;
    bool stillExists = m_service.getAircraftById(newAc.id, deletedAc);
    QVERIFY(!stillExists);
}

void TestAircraftManagement::testLiveRejectDuplicateRegistration()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping duplicate test.");
    }

    Session adminSession(1, QStringLiteral("admin_demo"), QStringLiteral("Admin User"), QStringLiteral("Admin"));
    m_service.setTestSession(adminSession);

    // Try creating an aircraft with existing registration VT-AT1
    Models::Aircraft dupAc;
    dupAc.registrationNumber = QStringLiteral("VT-AT1");
    dupAc.aircraftType = QStringLiteral("Boeing 737");
    dupAc.capacity = 150;
    dupAc.status = QStringLiteral("Active");

    QString error;
    bool created = m_service.createAircraft(dupAc, &error);
    QVERIFY(!created);
    QVERIFY(error.contains(QStringLiteral("already registered"), Qt::CaseInsensitive));
}

void TestAircraftManagement::testLiveOperatorStatusUpdateOnly()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping operator update test.");
    }

    // 1. Create a test aircraft as Admin
    Session adminSession(1, QStringLiteral("admin_demo"), QStringLiteral("Admin User"), QStringLiteral("Admin"));
    m_service.setTestSession(adminSession);

    Models::Aircraft testAc;
    testAc.registrationNumber = QStringLiteral("VT-TEST200");
    testAc.aircraftType = QStringLiteral("ATR 72-600");
    testAc.capacity = 70;
    testAc.status = QStringLiteral("Active");

    QString err;
    QVERIFY(m_service.createAircraft(testAc, &err));

    // 2. Switch to Airport Operator
    Session opSession(3, QStringLiteral("operator_demo"), QStringLiteral("Operator User"), QStringLiteral("Airport Operator"));
    m_service.setTestSession(opSession);

    // Operator attempts to illegally tamper with type and capacity -> Must be rejected!
    Models::Aircraft tamperedAc = testAc;
    tamperedAc.aircraftType = QStringLiteral("Tampered Type");
    tamperedAc.capacity = 999;
    tamperedAc.status = QStringLiteral("Maintenance");

    bool tamperedResult = m_service.updateAircraft(tamperedAc, &err);
    QVERIFY(!tamperedResult);
    QVERIFY(err.contains(QStringLiteral("Airport Operators may only update aircraft operational status"), Qt::CaseInsensitive));

    // Operator updates only operational status -> Must succeed!
    testAc.status = QStringLiteral("Maintenance");
    bool updated = m_service.updateAircraft(testAc, &err);
    QVERIFY2(updated, qPrintable(err));

    // 3. Verify that status changed, while type and capacity remain intact
    Models::Aircraft checkAc;
    m_service.getAircraftById(testAc.id, checkAc);
    QCOMPARE(checkAc.status, QStringLiteral("Maintenance"));
    QCOMPARE(checkAc.aircraftType, QStringLiteral("ATR 72-600"));
    QCOMPARE(checkAc.capacity, 70);

    // Operator updates status via dedicated API
    bool statusUpdated = m_service.updateAircraftStatus(testAc.id, QStringLiteral("Retired"), &err);
    QVERIFY2(statusUpdated, qPrintable(err));

    m_service.getAircraftById(testAc.id, checkAc);
    QCOMPARE(checkAc.status, QStringLiteral("Retired"));

    // Cleanup as Admin
    m_service.setTestSession(adminSession);
    m_service.deleteAircraft(testAc.id, &err);
}

void TestAircraftManagement::testLiveReferentialIntegrityPreventsReferencedDeletion()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping referential integrity test.");
    }

    Session adminSession(1, QStringLiteral("admin_demo"), QStringLiteral("Admin User"), QStringLiteral("Admin"));
    m_service.setTestSession(adminSession);

    // Aircraft ID 1 (VT-AT1) is assigned to flight AI-101 in seed data
    QString reason;
    bool canDelete = m_service.canDeleteAircraft(1, &reason);
    QVERIFY(!canDelete);
    QVERIFY(reason.contains(QStringLiteral("referenced by"), Qt::CaseInsensitive));

    // Attempting deleteAircraft(1) must fail safely
    QString deleteErr;
    bool deleted = m_service.deleteAircraft(1, &deleteErr);
    QVERIFY(!deleted);
    QVERIFY(deleteErr.contains(QStringLiteral("Cannot delete aircraft"), Qt::CaseInsensitive));

    // Verify VT-AT1 still exists intact in the database
    Models::Aircraft ac;
    bool stillExists = m_service.getAircraftById(1, ac);
    QVERIFY(stillExists);
    QCOMPARE(ac.registrationNumber, QStringLiteral("VT-AT1"));
}

void TestAircraftManagement::testLiveAvailableAircraftForFlightAssignment()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not connected; skipping available aircraft test.");
    }

    Session atcSession(2, QStringLiteral("atc_demo"), QStringLiteral("Radar Controller"), QStringLiteral("ATC"));
    m_service.setTestSession(atcSession);

    // Aircraft options for flight assignment dialog
    QList<QPair<int, QString>> available = m_service.getAvailableAircraft();
    QVERIFY(!available.isEmpty());

    // VT-AT1 is Active, so it must be present
    bool foundVTAT1 = false;
    for (const auto &pair : available) {
        if (pair.second.contains(QStringLiteral("VT-AT1"))) {
            foundVTAT1 = true;
            break;
        }
    }
    QVERIFY2(foundVTAT1, "Active aircraft VT-AT1 should be available for flight assignment");
}

QTEST_MAIN(TestAircraftManagement)
#include "TestAircraftManagement.moc"
