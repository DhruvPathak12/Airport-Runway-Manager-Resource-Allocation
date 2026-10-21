#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>

#include "models/Runway.h"
#include "models/Session.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"

class TestRunwayManagement : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ── 1. Model & Validation Tests ──────────────────────────────────────────
    void testValidRunwayValidation();
    void testEmptyRunwayNumberRejection();
    void testExcessiveRunwayNumberLength();
    void testValidRunwayTypesAccepted();
    void testInvalidRunwayTypeRejection();
    void testValidStatusesAccepted();
    void testInvalidStatusRejection();
    void testTrimmingAndNormalization();
    void testStatusAndTypeEnumConversions();

    // ── 2. Role-Based Access Control at Service Boundary ─────────────────────
    void testUnauthenticatedCannotAccessOrModify();
    void testUnknownRoleDeniedAccess();
    void testATCRoleViewOnlyDeniedMaintenance();
    void testAirportOperatorPermittedMaintenance();
    void testAirportOperatorDeniedManualOccupied();
    void testAdminRolePermittedFullRunwayAdministration();

    // ── 3. Live MySQL CRUD & Retrieval Tests ──────────────────────────────────
    void testLiveGetAllRunways();
    void testLiveRunwaySearchAndStatusFilter();
    void testLiveGetRunwayById();
    void testLiveGetRunwayByNumber();
    void testLiveNonExistentRunwayHandling();
    void testLiveSetRunwayMaintenanceStatus();
    void testLiveAdminCreateAndModifyRunwayCycle();
    void testLiveRejectDuplicateRunwayNumber();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;
};

void TestRunwayManagement::initTestCase()
{
    m_service.initialize();
    m_dbConnected = DatabaseManager::instance().isConnected();
    if (!m_dbConnected) {
        qWarning() << "[TestRunwayManagement] Live database connection unavailable; testing offline functionality only.";
    }
}

void TestRunwayManagement::cleanupTestCase()
{
    if (m_dbConnected) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            // Clean up any test runways
            q.exec(QStringLiteral("DELETE FROM runways WHERE runway_number LIKE 'RW-TEST%';"));
        }
    }
    m_service.shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Model & Validation Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestRunwayManagement::testValidRunwayValidation()
{
    Models::Runway rw;
    rw.runwayNumber = QStringLiteral("09L");
    rw.runwayType = QStringLiteral("Both");
    rw.status = QStringLiteral("Available");

    QString error;
    QVERIFY2(rw.validate(&error), qPrintable(error));
    QVERIFY(error.isEmpty());
    QVERIFY(rw.isAvailable());
    QCOMPARE(rw.isOccupied(), false);
    QCOMPARE(rw.isMaintenance(), false);
}

void TestRunwayManagement::testEmptyRunwayNumberRejection()
{
    Models::Runway rw;
    rw.runwayNumber = QStringLiteral("   ");
    rw.runwayType = QStringLiteral("Both");
    rw.status = QStringLiteral("Available");

    QString error;
    QCOMPARE(rw.validate(&error), false);
    QVERIFY(error.contains(QStringLiteral("required"), Qt::CaseInsensitive));
}

void TestRunwayManagement::testExcessiveRunwayNumberLength()
{
    Models::Runway rw;
    rw.runwayNumber = QStringLiteral("RWY-1234567890"); // 14 chars, exceeds 10
    rw.runwayType = QStringLiteral("Landing");
    rw.status = QStringLiteral("Available");

    QString error;
    QCOMPARE(rw.validate(&error), false);
    QVERIFY(error.contains(QStringLiteral("cannot exceed 10"), Qt::CaseInsensitive));
}

void TestRunwayManagement::testValidRunwayTypesAccepted()
{
    const QStringList types = {
        QStringLiteral("Landing"),
        QStringLiteral("Takeoff"),
        QStringLiteral("Both")
    };

    for (const QString &type : types) {
        Models::Runway rw(1, QStringLiteral("27R"), type, QStringLiteral("Available"));
        QString error;
        QVERIFY2(rw.validate(&error), qPrintable(error));
    }
}

void TestRunwayManagement::testInvalidRunwayTypeRejection()
{
    Models::Runway rw(1, QStringLiteral("27R"), QStringLiteral("Taxiway"), QStringLiteral("Available"));
    QString error;
    QCOMPARE(rw.validate(&error), false);
    QVERIFY(error.contains(QStringLiteral("Invalid runway type"), Qt::CaseInsensitive));
}

void TestRunwayManagement::testValidStatusesAccepted()
{
    const QStringList statuses = {
        QStringLiteral("Available"),
        QStringLiteral("Occupied"),
        QStringLiteral("Maintenance")
    };

    for (const QString &status : statuses) {
        Models::Runway rw(1, QStringLiteral("14"), QStringLiteral("Both"), status);
        QString error;
        QVERIFY2(rw.validate(&error), qPrintable(error));
    }
}

void TestRunwayManagement::testInvalidStatusRejection()
{
    Models::Runway rw(1, QStringLiteral("14"), QStringLiteral("Both"), QStringLiteral("ClosedIndefinitely"));
    QString error;
    QCOMPARE(rw.validate(&error), false);
    QVERIFY(error.contains(QStringLiteral("Invalid runway status"), Qt::CaseInsensitive));
}

void TestRunwayManagement::testTrimmingAndNormalization()
{
    Models::Runway rw;
    rw.runwayNumber = QStringLiteral("  09r  ");
    rw.runwayType = QStringLiteral("  takeoff  ");
    rw.status = QStringLiteral("  Available  ");
    rw.normalize();

    QCOMPARE(rw.runwayNumber, QStringLiteral("09R"));
    QCOMPARE(rw.effectiveNumber(), QStringLiteral("09R"));
    QCOMPARE(rw.name, QStringLiteral("09R"));
    QCOMPARE(rw.status, QStringLiteral("Available"));
}

void TestRunwayManagement::testStatusAndTypeEnumConversions()
{
    QCOMPARE(Models::Runway::stringToStatus(QStringLiteral("Available")), Models::RunwayStatus::Available);
    QCOMPARE(Models::Runway::stringToStatus(QStringLiteral("Occupied")), Models::RunwayStatus::Occupied);
    QCOMPARE(Models::Runway::stringToStatus(QStringLiteral("Maintenance")), Models::RunwayStatus::Maintenance);
    QCOMPARE(Models::Runway::stringToStatus(QStringLiteral("Garbage")), Models::RunwayStatus::Unknown);

    QCOMPARE(Models::Runway::statusToString(Models::RunwayStatus::Available), QStringLiteral("Available"));
    QCOMPARE(Models::Runway::statusToString(Models::RunwayStatus::Occupied), QStringLiteral("Occupied"));
    QCOMPARE(Models::Runway::statusToString(Models::RunwayStatus::Maintenance), QStringLiteral("Maintenance"));

    QCOMPARE(Models::Runway::stringToType(QStringLiteral("Landing")), Models::RunwayType::Landing);
    QCOMPARE(Models::Runway::stringToType(QStringLiteral("Takeoff")), Models::RunwayType::Takeoff);
    QCOMPARE(Models::Runway::stringToType(QStringLiteral("Both")), Models::RunwayType::Both);
    QCOMPARE(Models::Runway::stringToType(QStringLiteral("Other")), Models::RunwayType::Unknown);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Role-Based Access Control at Service Boundary
// ─────────────────────────────────────────────────────────────────────────────

void TestRunwayManagement::testUnauthenticatedCannotAccessOrModify()
{
    ApplicationService service;
    service.logout();
    QCOMPARE(service.isAuthenticated(), false);

    QString err;
    QVERIFY(service.getAllRunways(QString(), QString(), &err).isEmpty());
    QVERIFY(err.contains(QStringLiteral("Authentication required")));

    Models::Runway outRw;
    QCOMPARE(service.getRunwayById(1, outRw, &err), false);
    QVERIFY(err.contains(QStringLiteral("Authentication required")));

    QCOMPARE(service.updateRunwayStatus(1, QStringLiteral("Maintenance"), &err), false);
    QVERIFY(err.contains(QStringLiteral("Authentication required")));

    QCOMPARE(service.setRunwayMaintenanceStatus(1, true, &err), false);
    QVERIFY(err.contains(QStringLiteral("Authentication required")));

    Models::Runway newRw(0, QStringLiteral("99L"));
    QCOMPARE(service.createRunway(newRw, &err), false);
    QVERIFY(err.contains(QStringLiteral("Authentication required")));
}

void TestRunwayManagement::testUnknownRoleDeniedAccess()
{
    ApplicationService service;
    Session visitor(999, QStringLiteral("visitor"), QStringLiteral("Guest Visitor"), QStringLiteral("Visitor"));
    service.setTestSession(visitor);

    QString err;
    QVERIFY(service.getAllRunways(QString(), QString(), &err).isEmpty());
    QVERIFY(err.contains(QStringLiteral("Access denied")));

    Models::Runway rw;
    QCOMPARE(service.getRunwayById(1, rw, &err), false);
    QVERIFY(err.contains(QStringLiteral("Access denied")));

    QCOMPARE(service.updateRunwayStatus(1, QStringLiteral("Maintenance"), &err), false);
    QVERIFY(err.contains(QStringLiteral("Access denied")));
}

void TestRunwayManagement::testATCRoleViewOnlyDeniedMaintenance()
{
    ApplicationService service;
    Session atc(2, QStringLiteral("atc_controller"), QStringLiteral("Radar Controller"), QStringLiteral("ATC"));
    service.setTestSession(atc);

    QVERIFY(service.currentSession().hasPermission(QStringLiteral("page:Runways")));
    QCOMPARE(service.currentSession().hasPermission(QStringLiteral("action:RunwayMaintenance")), false);

    QString err;
    // ATC CANNOT perform runway maintenance
    QCOMPARE(service.setRunwayMaintenanceStatus(1, true, &err), false);
    QVERIFY(err.contains(QStringLiteral("Access denied")));

    // ATC CANNOT update runway status
    QCOMPARE(service.updateRunwayStatus(1, QStringLiteral("Maintenance"), &err), false);
    QVERIFY(err.contains(QStringLiteral("Access denied")));

    // ATC CANNOT create runway records
    Models::Runway newRw(0, QStringLiteral("99L"));
    QCOMPARE(service.createRunway(newRw, &err), false);
    QVERIFY(err.contains(QStringLiteral("Access denied")));
}

void TestRunwayManagement::testAirportOperatorPermittedMaintenance()
{
    ApplicationService service;
    Session op(3, QStringLiteral("operator_user"), QStringLiteral("Ground Marshall"), QStringLiteral("Airport Operator"));
    service.setTestSession(op);

    QVERIFY(service.currentSession().hasPermission(QStringLiteral("page:Runways")));
    QVERIFY(service.currentSession().hasPermission(QStringLiteral("action:RunwayMaintenance")));

    QString err;
    // Operator CAN set runway maintenance
    bool ok = service.setRunwayMaintenanceStatus(1, true, &err);
    QVERIFY2(ok, qPrintable(err));
}

void TestRunwayManagement::testAirportOperatorDeniedManualOccupied()
{
    ApplicationService service;
    Session op(3, QStringLiteral("operator_user"), QStringLiteral("Ground Marshall"), QStringLiteral("Airport Operator"));
    service.setTestSession(op);

    QString err;
    // Airport Operator cannot manually set a runway to Occupied (occupancy is through allocation)
    QCOMPARE(service.updateRunwayStatus(1, QStringLiteral("Occupied"), &err), false);
    QVERIFY(err.contains(QStringLiteral("cannot manually set runway to Occupied")));
}

void TestRunwayManagement::testAdminRolePermittedFullRunwayAdministration()
{
    ApplicationService service;
    Session admin(1, QStringLiteral("admin_user"), QStringLiteral("System Admin"), QStringLiteral("Admin"));
    service.setTestSession(admin);

    QVERIFY(service.currentSession().hasPermission(QStringLiteral("page:Runways")));
    QVERIFY(service.currentSession().isAdmin());
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Live MySQL CRUD & Retrieval Tests
// ─────────────────────────────────────────────────────────────────────────────

void TestRunwayManagement::testLiveGetAllRunways()
{
    if (!m_dbConnected) {
        QSKIP("Live database unavailable.");
    }

    Session admin(1, QStringLiteral("admin_demo"), QStringLiteral("Admin"), QStringLiteral("Admin"));
    m_service.setTestSession(admin);

    QString error;
    QList<Models::Runway> runways = m_service.getAllRunways(QString(), QString(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    // Seed has 3 runways: 09L, 09R, 27
    QVERIFY(runways.size() >= 3);

    bool found09L = false;
    for (const auto &rw : runways) {
        if (rw.runwayNumber == QStringLiteral("09L")) {
            found09L = true;
            QVERIFY(rw.id > 0);
        }
    }
    QVERIFY(found09L);
}

void TestRunwayManagement::testLiveRunwaySearchAndStatusFilter()
{
    if (!m_dbConnected) {
        QSKIP("Live database unavailable.");
    }

    Session admin(1, QStringLiteral("admin_demo"), QStringLiteral("Admin"), QStringLiteral("Admin"));
    m_service.setTestSession(admin);

    QString error;
    // Search "09" should find both 09L and 09R
    QList<Models::Runway> search09 = m_service.getAllRunways(QStringLiteral("09"), QString(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(search09.size() >= 2);

    // Filter by "Available"
    QList<Models::Runway> avail = m_service.getAllRunways(QString(), QStringLiteral("Available"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    for (const auto &rw : avail) {
        QCOMPARE(rw.status, QStringLiteral("Available"));
    }
}

void TestRunwayManagement::testLiveGetRunwayById()
{
    if (!m_dbConnected) {
        QSKIP("Live database unavailable.");
    }

    Session atc(2, QStringLiteral("atc_demo"), QStringLiteral("ATC"), QStringLiteral("ATC"));
    m_service.setTestSession(atc);

    QString error;
    QList<Models::Runway> all = m_service.getAllRunways(QString(), QString(), &error);
    QVERIFY(!all.isEmpty());

    int targetId = all.first().id;
    Models::Runway fetched;
    QVERIFY(m_service.getRunwayById(targetId, fetched, &error));
    QCOMPARE(fetched.id, targetId);
    QCOMPARE(fetched.runwayNumber, all.first().runwayNumber);
}

void TestRunwayManagement::testLiveGetRunwayByNumber()
{
    if (!m_dbConnected) {
        QSKIP("Live database unavailable.");
    }

    Session op(3, QStringLiteral("operator_demo"), QStringLiteral("Operator"), QStringLiteral("Airport Operator"));
    m_service.setTestSession(op);

    QString error;
    Models::Runway rw;
    QVERIFY(m_service.getRunwayByNumber(QStringLiteral("09L"), rw, &error));
    QCOMPARE(rw.runwayNumber, QStringLiteral("09L"));
}

void TestRunwayManagement::testLiveNonExistentRunwayHandling()
{
    if (!m_dbConnected) {
        QSKIP("Live database unavailable.");
    }

    Session admin(1, QStringLiteral("admin_demo"), QStringLiteral("Admin"), QStringLiteral("Admin"));
    m_service.setTestSession(admin);

    QString error;
    Models::Runway rw;
    // Invalid ID 999999
    QCOMPARE(m_service.getRunwayById(999999, rw, &error), false);
    QVERIFY(error.contains(QStringLiteral("not found"), Qt::CaseInsensitive));

    // Invalid number
    QCOMPARE(m_service.getRunwayByNumber(QStringLiteral("NONEXIST99"), rw, &error), false);
    QVERIFY(error.contains(QStringLiteral("not found"), Qt::CaseInsensitive));
}

void TestRunwayManagement::testLiveSetRunwayMaintenanceStatus()
{
    if (!m_dbConnected) {
        QSKIP("Live database unavailable.");
    }

    Session op(3, QStringLiteral("operator_demo"), QStringLiteral("Operator"), QStringLiteral("Airport Operator"));
    m_service.setTestSession(op);

    QString error;
    Models::Runway rw09R;
    QVERIFY(m_service.getRunwayByNumber(QStringLiteral("09R"), rw09R, &error));
    QString originalStatus = rw09R.status;

    // Toggle to Maintenance
    QVERIFY(m_service.setRunwayMaintenanceStatus(rw09R.id, true, &error));
    Models::Runway updated;
    QVERIFY(m_service.getRunwayById(rw09R.id, updated, &error));
    QCOMPARE(updated.status, QStringLiteral("Maintenance"));

    // Toggle back to Available
    QVERIFY(m_service.setRunwayMaintenanceStatus(rw09R.id, false, &error));
    QVERIFY(m_service.getRunwayById(rw09R.id, updated, &error));
    QCOMPARE(updated.status, QStringLiteral("Available"));

    // Restore original if was different
    if (originalStatus != QStringLiteral("Available")) {
        m_service.updateRunwayStatus(rw09R.id, originalStatus, &error);
    }
}

void TestRunwayManagement::testLiveAdminCreateAndModifyRunwayCycle()
{
    if (!m_dbConnected) {
        QSKIP("Live database unavailable.");
    }

    Session admin(1, QStringLiteral("admin_demo"), QStringLiteral("Admin"), QStringLiteral("Admin"));
    m_service.setTestSession(admin);

    QString error;
    Models::Runway newRw;
    newRw.runwayNumber = QStringLiteral("RW-TEST1");
    newRw.runwayType = QStringLiteral("Landing");
    newRw.status = QStringLiteral("Available");

    // Create
    QVERIFY2(m_service.createRunway(newRw, &error), qPrintable(error));

    // Retrieve
    Models::Runway fetched;
    QVERIFY(m_service.getRunwayByNumber(QStringLiteral("RW-TEST1"), fetched, &error));
    QCOMPARE(fetched.runwayNumber, QStringLiteral("RW-TEST1"));
    QCOMPARE(fetched.runwayType, QStringLiteral("Landing"));
    QCOMPARE(fetched.status, QStringLiteral("Available"));

    // Update
    fetched.runwayType = QStringLiteral("Both");
    fetched.status = QStringLiteral("Maintenance");
    QVERIFY(m_service.updateRunway(fetched, &error));

    // Verify update
    Models::Runway updated;
    QVERIFY(m_service.getRunwayById(fetched.id, updated, &error));
    QCOMPARE(updated.runwayType, QStringLiteral("Both"));
    QCOMPARE(updated.status, QStringLiteral("Maintenance"));

    // Clean up
    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery del(db);
    del.prepare(QStringLiteral("DELETE FROM runways WHERE runway_number = 'RW-TEST1';"));
    del.exec();
}

void TestRunwayManagement::testLiveRejectDuplicateRunwayNumber()
{
    if (!m_dbConnected) {
        QSKIP("Live database unavailable.");
    }

    Session admin(1, QStringLiteral("admin_demo"), QStringLiteral("Admin"), QStringLiteral("Admin"));
    m_service.setTestSession(admin);

    QString error;
    // Runway 09L already exists
    Models::Runway dup;
    dup.runwayNumber = QStringLiteral("09L");
    dup.runwayType = QStringLiteral("Both");
    dup.status = QStringLiteral("Available");

    QCOMPARE(m_service.createRunway(dup, &error), false);
    QVERIFY(error.contains(QStringLiteral("already exists"), Qt::CaseInsensitive));
}

QTEST_MAIN(TestRunwayManagement)
#include "TestRunwayManagement.moc"
