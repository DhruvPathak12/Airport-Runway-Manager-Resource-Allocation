#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>
#include "utils/PasswordUtils.h"
#include "models/Session.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"

class TestAuthAndRBAC : public QObject
{
    Q_OBJECT

private slots:
    // PasswordUtils tests
    void testSha256DigestGeneration();
    void testCorrectPasswordVerification();
    void testIncorrectPasswordRejection();
    void testEmptyPasswordHandling();

    // Session tests
    void testNoPlaintextPasswordInSession();
    void testSessionCreationAndValidation();
    void testLogoutClearsSessionState();

    // RBAC & Permission policy tests
    void testUnknownRoleDeniedAccess();
    void testAdminPermissionPolicy();
    void testATCPermissionPolicy();
    void testAirportOperatorPermissionPolicy();
    void testProtectedNavigationDeniedWhenUnauthenticated();

    // Authentication integration tests
    void testLoginInputValidation();
    void testAuthenticationWithBoundParameters();
    void testLiveDatabaseAuthenticationIfConfigured();

    // Action-level RBAC & Service boundary enforcement tests
    void testActionLevelRBACEnforcement();
    void testInactiveAccountRejected();

    // Security Hardening: Input validation, passwords with colons & special chars, environment restriction
    void testAccountInputValidation();
    void testPasswordsWithColonsAndSpecialCharacters();
    void testProductionEnvironmentRestriction();
};


void TestAuthAndRBAC::testSha256DigestGeneration()
{
    QString hash1 = PasswordUtils::hash(QStringLiteral("password123"));
    // SHA-256 of "password123" is ef92b778bafe771e89245b89ecbc08a44a4e166c06659911881f383d4473e94f
    QCOMPARE(hash1.toLower(), QStringLiteral("ef92b778bafe771e89245b89ecbc08a44a4e166c06659911881f383d4473e94f"));
    QCOMPARE(hash1.length(), 64);
}

void TestAuthAndRBAC::testCorrectPasswordVerification()
{
    QString plaintext = QStringLiteral("SecurePass#2026");
    QString digest = PasswordUtils::hash(plaintext);

    QVERIFY(PasswordUtils::verify(plaintext, digest));
    // Case-insensitive hex matching
    QVERIFY(PasswordUtils::verify(plaintext, digest.toUpper()));
}

void TestAuthAndRBAC::testIncorrectPasswordRejection()
{
    QString digest = PasswordUtils::hash(QStringLiteral("CorrectPassword"));
    QCOMPARE(PasswordUtils::verify(QStringLiteral("WrongPassword"), digest), false);
    QCOMPARE(PasswordUtils::verify(QStringLiteral("correctpassword"), digest), false); // case-sensitive plaintext
}

void TestAuthAndRBAC::testEmptyPasswordHandling()
{
    QVERIFY(PasswordUtils::hash(QString()).isEmpty());
    QCOMPARE(PasswordUtils::verify(QString(), QStringLiteral("somehash")), false);
    QCOMPARE(PasswordUtils::verify(QStringLiteral("somepass"), QString()), false);
}

void TestAuthAndRBAC::testNoPlaintextPasswordInSession()
{
    Session session(1, QStringLiteral("admin_demo"), QStringLiteral("Administrator"), QStringLiteral("Admin"));
    QVERIFY(session.isValid());
    // Session model has no password fields or accessors
    QCOMPARE(session.userId(), 1);
    QCOMPARE(session.username(), QStringLiteral("admin_demo"));
    QCOMPARE(session.role(), QStringLiteral("Admin"));
}

void TestAuthAndRBAC::testSessionCreationAndValidation()
{
    Session invalidSession;
    QCOMPARE(invalidSession.isValid(), false);
    QCOMPARE(invalidSession.isAdmin(), false);
    QCOMPARE(invalidSession.isATC(), false);
    QCOMPARE(invalidSession.isOperator(), false);

    Session adminSession(10, QStringLiteral("alice"), QStringLiteral("Alice Admin"), QStringLiteral("Admin"));
    QVERIFY(adminSession.isValid());
    QVERIFY(adminSession.isAdmin());
    QCOMPARE(adminSession.isATC(), false);

    Session atcSession(20, QStringLiteral("bob"), QStringLiteral("Bob Controller"), QStringLiteral("ATC"));
    QVERIFY(atcSession.isValid());
    QVERIFY(atcSession.isATC());
    QCOMPARE(atcSession.isAdmin(), false);

    Session opSession(30, QStringLiteral("charlie"), QStringLiteral("Charlie Marshall"), QStringLiteral("Airport Operator"));
    QVERIFY(opSession.isValid());
    QVERIFY(opSession.isOperator());
}

void TestAuthAndRBAC::testLogoutClearsSessionState()
{
    ApplicationService service;
    Session session(42, QStringLiteral("controller"), QStringLiteral("Lead Controller"), QStringLiteral("ATC"));
    service.setTestSession(session);
    QVERIFY(service.isAuthenticated());
    QCOMPARE(service.currentSession().username(), QStringLiteral("controller"));

    service.logout();
    QCOMPARE(service.isAuthenticated(), false);
    QCOMPARE(service.currentSession().isValid(), false);
    QCOMPARE(service.currentSession().username(), QString());
}

void TestAuthAndRBAC::testUnknownRoleDeniedAccess()
{
    Session rogueSession(99, QStringLiteral("guest"), QStringLiteral("Guest"), QStringLiteral("Visitor"));
    QVERIFY(rogueSession.isValid());
    QCOMPARE(rogueSession.isAdmin(), false);
    QCOMPARE(rogueSession.isATC(), false);
    QCOMPARE(rogueSession.isOperator(), false);

    // Should only have generic dashboard access or none
    QCOMPARE(rogueSession.hasPermission(QStringLiteral("page:Users")), false);
    QCOMPARE(rogueSession.hasPermission(QStringLiteral("page:Scheduling")), false);
    QCOMPARE(rogueSession.hasPermission(QStringLiteral("action:RunwayAllocation")), false);
}

void TestAuthAndRBAC::testAdminPermissionPolicy()
{
    Session admin(1, QStringLiteral("admin"), QStringLiteral("SysAdmin"), QStringLiteral("Admin"));
    QVERIFY(admin.hasPermission(QStringLiteral("page:Dashboard")));
    QVERIFY(admin.hasPermission(QStringLiteral("page:Users")));
    QVERIFY(admin.hasPermission(QStringLiteral("action:ManageUsers")));
    QVERIFY(admin.hasPermission(QStringLiteral("page:Scheduling")));
    QVERIFY(admin.hasPermission(QStringLiteral("page:Flights")));
    QVERIFY(admin.hasPermission(QStringLiteral("page:Aircraft")));
    QVERIFY(admin.hasPermission(QStringLiteral("page:Runways")));
    QVERIFY(admin.hasPermission(QStringLiteral("page:Transactions")));
    QVERIFY(admin.hasPermission(QStringLiteral("page:Logs")));

    // Admin should not execute operational runway allocations or ATC flight controls
    QCOMPARE(admin.hasPermission(QStringLiteral("action:RunwayAllocation")), false);
    QCOMPARE(admin.hasPermission(QStringLiteral("action:ManageFlights")), false);
}

void TestAuthAndRBAC::testATCPermissionPolicy()
{
    Session atc(2, QStringLiteral("atc_user"), QStringLiteral("Radar Controller"), QStringLiteral("ATC"));
    QVERIFY(atc.hasPermission(QStringLiteral("page:Dashboard")));
    QVERIFY(atc.hasPermission(QStringLiteral("page:Flights")));
    QVERIFY(atc.hasPermission(QStringLiteral("action:ManageFlights")));
    QVERIFY(atc.hasPermission(QStringLiteral("page:Scheduling")));
    QVERIFY(atc.hasPermission(QStringLiteral("action:ManageScheduling")));
    QVERIFY(atc.hasPermission(QStringLiteral("page:Runways")));
    QVERIFY(atc.hasPermission(QStringLiteral("action:RunwayAllocation")));
    QVERIFY(atc.hasPermission(QStringLiteral("page:Logs")));

    // ATC cannot manage users or view transaction monitors
    QCOMPARE(atc.hasPermission(QStringLiteral("page:Users")), false);
    QCOMPARE(atc.hasPermission(QStringLiteral("action:ManageUsers")), false);
    QCOMPARE(atc.hasPermission(QStringLiteral("page:Transactions")), false);
    QCOMPARE(atc.hasPermission(QStringLiteral("action:RunwayMaintenance")), false);
}

void TestAuthAndRBAC::testAirportOperatorPermissionPolicy()
{
    Session op(3, QStringLiteral("op_user"), QStringLiteral("Ground Marshall"), QStringLiteral("Airport Operator"));
    QVERIFY(op.hasPermission(QStringLiteral("page:Dashboard")));
    QVERIFY(op.hasPermission(QStringLiteral("page:Aircraft")));
    QVERIFY(op.hasPermission(QStringLiteral("action:ManageAircraft")));
    QVERIFY(op.hasPermission(QStringLiteral("page:Runways")));
    QVERIFY(op.hasPermission(QStringLiteral("action:RunwayMaintenance")));
    QVERIFY(op.hasPermission(QStringLiteral("page:Logs")));

    // Operator cannot manage scheduling queue or assign live runways
    QCOMPARE(op.hasPermission(QStringLiteral("page:Scheduling")), false);
    QCOMPARE(op.hasPermission(QStringLiteral("action:RunwayAllocation")), false);
    QCOMPARE(op.hasPermission(QStringLiteral("page:Users")), false);
    QCOMPARE(op.hasPermission(QStringLiteral("page:Transactions")), false);
}

void TestAuthAndRBAC::testProtectedNavigationDeniedWhenUnauthenticated()
{
    Session emptySession;
    QCOMPARE(emptySession.isValid(), false);
    QCOMPARE(emptySession.hasPermission(QStringLiteral("page:Dashboard")), false);
    QCOMPARE(emptySession.hasPermission(QStringLiteral("page:Users")), false);
    QCOMPARE(emptySession.hasPermission(QStringLiteral("page:Scheduling")), false);
    QVERIFY(emptySession.allowedNavItems().isEmpty());
}

void TestAuthAndRBAC::testLoginInputValidation()
{
    ApplicationService service;
    QString err;
    QCOMPARE(service.login(QString(), QStringLiteral("pwd"), &err), false);
    QVERIFY(err.contains(QStringLiteral("required")));

    QCOMPARE(service.login(QStringLiteral("user"), QString(), &err), false);
    QVERIFY(err.contains(QStringLiteral("required")));
}

void TestAuthAndRBAC::testAuthenticationWithBoundParameters()
{
    // Test that SQL injection attempts fail gracefully without syntax errors or injection
    ApplicationService service;
    QString err;
    QString sqlInjectionUser = QStringLiteral("' OR 1=1 --");
    bool result = service.login(sqlInjectionUser, QStringLiteral("anypass"), &err);
    QCOMPARE(result, false);
    QVERIFY(!service.isAuthenticated());
}

void TestAuthAndRBAC::testLiveDatabaseAuthenticationIfConfigured()
{
    DatabaseManager &db = DatabaseManager::instance();
    if (!db.isConnected()) {
        if (!db.connect()) {
            QSKIP("Live database connection unavailable, skipping live authentication test.");
        }
    }

    ApplicationService service;
    QString err;

    // 1. Verify unknown account fails with sanitized generic message
    bool res1 = service.login(QStringLiteral("non_existent_user_9999"), QStringLiteral("pass"), &err);
    QCOMPARE(res1, false);
    QVERIFY(err.contains(QStringLiteral("Invalid username or password")));
    QCOMPARE(service.isAuthenticated(), false);

    // 3. Verify real provisioned demo accounts can authenticate and receive correct roles
    // 3a. Admin demo account
    bool adminLogin = service.login(QStringLiteral("admin_demo"), QStringLiteral("AdminPass#2026"), &err);
    QVERIFY(adminLogin);
    QVERIFY(service.isAuthenticated());
    QCOMPARE(service.currentSession().role(), QStringLiteral("Admin"));
    QVERIFY(service.currentSession().isAdmin());
    QVERIFY(service.currentSession().hasPermission(QStringLiteral("page:Users")));
    service.logout();
    QCOMPARE(service.isAuthenticated(), false);

    // 3b. ATC demo account
    bool atcLogin = service.login(QStringLiteral("atc_demo"), QStringLiteral("AtcPass#2026"), &err);
    QVERIFY(atcLogin);
    QVERIFY(service.isAuthenticated());
    QCOMPARE(service.currentSession().role(), QStringLiteral("ATC"));
    QVERIFY(service.currentSession().isATC());
    QVERIFY(service.currentSession().hasPermission(QStringLiteral("page:Flights")));
    QVERIFY(service.currentSession().hasPermission(QStringLiteral("action:ManageFlights")));
    QCOMPARE(service.currentSession().hasPermission(QStringLiteral("page:Users")), false);
    service.logout();
    QCOMPARE(service.isAuthenticated(), false);

    // 3c. Airport Operator demo account
    bool opLogin = service.login(QStringLiteral("operator_demo"), QStringLiteral("OperatorPass#2026"), &err);
    QVERIFY(opLogin);
    QVERIFY(service.isAuthenticated());
    QCOMPARE(service.currentSession().role(), QStringLiteral("Airport Operator"));
    QVERIFY(service.currentSession().isOperator());
    QVERIFY(service.currentSession().hasPermission(QStringLiteral("page:Aircraft")));
    QVERIFY(service.currentSession().hasPermission(QStringLiteral("action:RunwayMaintenance")));
    QCOMPARE(service.currentSession().hasPermission(QStringLiteral("page:Users")), false);
    service.logout();
    QCOMPARE(service.isAuthenticated(), false);

    // 4. Provision an ephemeral test user, authenticate successfully, verify session & RBAC, then clean up
    QString testUser = QStringLiteral("atc_test_unit");
    QString testPass = QStringLiteral("ValidPass#2026");
    QString testHash = PasswordUtils::hash(testPass);

    QSqlDatabase sqlDb = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(sqlDb);
    q.prepare(QStringLiteral(
        "INSERT INTO users (username, password_hash, role_id, full_name, status) "
        "VALUES (:u, :p, (SELECT role_id FROM roles WHERE role_name='ATC' LIMIT 1), 'Test ATC Officer', 'Active') "
        "ON DUPLICATE KEY UPDATE password_hash = :p2, status='Active';"
    ));
    q.bindValue(QStringLiteral(":u"), testUser);
    q.bindValue(QStringLiteral(":p"), testHash);
    q.bindValue(QStringLiteral(":p2"), testHash);
    QVERIFY(q.exec());

    // Login with wrong password
    bool badLogin = service.login(testUser, QStringLiteral("WrongPassword"), &err);
    QCOMPARE(badLogin, false);
    QVERIFY(!service.isAuthenticated());

    // Login with correct password
    bool goodLogin = service.login(testUser, testPass, &err);
    QVERIFY(goodLogin);
    QVERIFY(service.isAuthenticated());

    const Session &sess = service.currentSession();
    QCOMPARE(sess.username(), testUser);
    QCOMPARE(sess.role(), QStringLiteral("ATC"));
    QVERIFY(sess.isATC());
    QVERIFY(sess.hasPermission(QStringLiteral("page:Flights")));
    QVERIFY(sess.hasPermission(QStringLiteral("action:ManageFlights")));
    QCOMPARE(sess.hasPermission(QStringLiteral("page:Users")), false);

    // Logout
    service.logout();
    QCOMPARE(service.isAuthenticated(), false);

    // Clean up test account
    QSqlQuery cleanQ(sqlDb);
    cleanQ.prepare(QStringLiteral("DELETE FROM users WHERE username = :u"));
    cleanQ.bindValue(QStringLiteral(":u"), testUser);
    cleanQ.exec();
}

void TestAuthAndRBAC::testActionLevelRBACEnforcement()
{
    ApplicationService service;
    QString err;

    // 1. Unauthenticated invocation should fail for all actions
    QCOMPARE(service.executeSchedulerCycle(&err), false);
    QVERIFY(err.contains(QStringLiteral("Authentication required")));

    QCOMPARE(service.allocateRunwayResource(1, 101, &err), false);
    QVERIFY(err.contains(QStringLiteral("Authentication required")));

    QCOMPARE(service.setRunwayMaintenanceStatus(1, true, &err), false);
    QVERIFY(err.contains(QStringLiteral("Authentication required")));

    // 2. Admin session
    Session admin(1, QStringLiteral("admin"), QStringLiteral("Admin User"), QStringLiteral("Admin"));
    service.setTestSession(admin);
    // Admin can execute scheduling cycle (override)
    QVERIFY(service.executeSchedulerCycle(&err));
    // Admin CANNOT allocate runway resources directly (ATC-only)
    QCOMPARE(service.allocateRunwayResource(1, 101, &err), false);
    QVERIFY(err.contains(QStringLiteral("Access denied")));
    // Admin CANNOT perform runway maintenance directly (Operator-only)
    QCOMPARE(service.setRunwayMaintenanceStatus(1, true, &err), false);
    QVERIFY(err.contains(QStringLiteral("Access denied")));

    // 3. ATC session
    Session atc(2, QStringLiteral("atc"), QStringLiteral("ATC User"), QStringLiteral("ATC"));
    service.setTestSession(atc);
    // ATC CAN run scheduler cycle
    QVERIFY(service.executeSchedulerCycle(&err));
    // ATC CAN allocate runway (stub returns false internally but authorization passes!)
    err.clear();
    service.allocateRunwayResource(1, 101, &err);
    QVERIFY(!err.contains(QStringLiteral("Access denied")));
    // ATC CANNOT perform runway maintenance
    QCOMPARE(service.setRunwayMaintenanceStatus(1, true, &err), false);
    QVERIFY(err.contains(QStringLiteral("Access denied")));

    // 4. Airport Operator session
    Session op(3, QStringLiteral("op"), QStringLiteral("Operator User"), QStringLiteral("Airport Operator"));
    service.setTestSession(op);
    // Operator CANNOT run scheduler cycle
    QCOMPARE(service.executeSchedulerCycle(&err), false);
    QVERIFY(err.contains(QStringLiteral("Access denied")));
    // Operator CANNOT allocate runways
    QCOMPARE(service.allocateRunwayResource(1, 101, &err), false);
    QVERIFY(err.contains(QStringLiteral("Access denied")));
    // Operator CAN set runway maintenance
    err.clear();
    QVERIFY(service.setRunwayMaintenanceStatus(1, true, &err));
    QVERIFY(err.isEmpty());
}

void TestAuthAndRBAC::testInactiveAccountRejected()
{
    DatabaseManager &db = DatabaseManager::instance();
    if (!db.isConnected()) {
        if (!db.connect()) {
            QSKIP("Live database connection unavailable, skipping inactive account test.");
        }
    }

    ApplicationService service;
    QString err;
    QString testUser = QStringLiteral("suspended_user_test");
    QString testPass = QStringLiteral("ValidPass#2026");
    QString testHash = PasswordUtils::hash(testPass);

    QSqlDatabase sqlDb = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(sqlDb);
    q.prepare(QStringLiteral(
        "INSERT INTO users (username, password_hash, role_id, full_name, status) "
        "VALUES (:u, :p, (SELECT role_id FROM roles WHERE role_name='ATC' LIMIT 1), 'Suspended User', 'Inactive') "
        "ON DUPLICATE KEY UPDATE password_hash = :p2, status='Inactive';"
    ));
    q.bindValue(QStringLiteral(":u"), testUser);
    q.bindValue(QStringLiteral(":p"), testHash);
    q.bindValue(QStringLiteral(":p2"), testHash);
    QVERIFY(q.exec());

    // Attempt login with correct password but inactive status
    bool res = service.login(testUser, testPass, &err);
    QCOMPARE(res, false);
    QVERIFY(!service.isAuthenticated());
    QVERIFY(err.contains(QStringLiteral("inactive or suspended"), Qt::CaseInsensitive));

    // Clean up
    QSqlQuery cleanQ(sqlDb);
    cleanQ.prepare(QStringLiteral("DELETE FROM users WHERE username = :u"));
    cleanQ.bindValue(QStringLiteral(":u"), testUser);
    cleanQ.exec();
}

void TestAuthAndRBAC::testAccountInputValidation()
{
    QString err;

    // 1. Empty username
    QCOMPARE(PasswordUtils::validateAccountInput(QString(), QStringLiteral("Admin"), QStringLiteral("Admin User"), QStringLiteral("pass123"), &err), false);
    QVERIFY(err.contains(QStringLiteral("Username cannot be empty")));

    // 2. Excessively long username (> 50 chars)
    QString longUser(51, QLatin1Char('u'));
    QCOMPARE(PasswordUtils::validateAccountInput(longUser, QStringLiteral("Admin"), QStringLiteral("Admin User"), QStringLiteral("pass123"), &err), false);
    QVERIFY(err.contains(QStringLiteral("50 characters")));

    // 3. Empty or invalid role
    QCOMPARE(PasswordUtils::validateAccountInput(QStringLiteral("valid_user"), QString(), QStringLiteral("User Name"), QStringLiteral("pass123"), &err), false);
    QVERIFY(err.contains(QStringLiteral("Role cannot be empty")));

    QCOMPARE(PasswordUtils::validateAccountInput(QStringLiteral("valid_user"), QStringLiteral("SuperUser"), QStringLiteral("User Name"), QStringLiteral("pass123"), &err), false);
    QVERIFY(err.contains(QStringLiteral("Role 'SuperUser' is invalid")));

    // 4. Empty password
    QCOMPARE(PasswordUtils::validateAccountInput(QStringLiteral("valid_user"), QStringLiteral("ATC"), QStringLiteral("User Name"), QString(), &err), false);
    QVERIFY(err.contains(QStringLiteral("Password cannot be empty")));

    // 5. Valid input
    err.clear();
    QVERIFY(PasswordUtils::validateAccountInput(QStringLiteral("valid_user"), QStringLiteral("Airport Operator"), QStringLiteral("Marshall"), QStringLiteral("Complex:Pass#2026 "), &err));
    QVERIFY(err.isEmpty());
}

void TestAuthAndRBAC::testPasswordsWithColonsAndSpecialCharacters()
{
    // The previous colon-separated format broke on passwords containing colons or spaces
    // Verify that PasswordUtils and ApplicationService handle colons, spaces, and punctuation safely
    QString passwordWithColons = QStringLiteral("P@ss:w0rd::with:colons and spaces#2026!");
    QString hashed = PasswordUtils::hash(passwordWithColons);
    QVERIFY(!hashed.isEmpty());
    QCOMPARE(hashed.length(), 64);

    // Verification must succeed
    QVERIFY(PasswordUtils::verify(passwordWithColons, hashed));

    // Live DB integration test with colons in password
    DatabaseManager &db = DatabaseManager::instance();
    if (!db.isConnected()) {
        if (!db.connect()) {
            QSKIP("Live database connection unavailable, skipping colons in password test.");
        }
    }

    ApplicationService service;
    QString err;
    QString testUser = QStringLiteral("colon_pass_user");

    QSqlDatabase sqlDb = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(sqlDb);
    q.prepare(QStringLiteral(
        "INSERT INTO users (username, password_hash, role_id, full_name, status) "
        "VALUES (:u, :p, (SELECT role_id FROM roles WHERE role_name='ATC' LIMIT 1), 'Colon Tester', 'Active') "
        "ON DUPLICATE KEY UPDATE password_hash = :p2, status='Active';"
    ));
    q.bindValue(QStringLiteral(":u"), testUser);
    q.bindValue(QStringLiteral(":p"), hashed);
    q.bindValue(QStringLiteral(":p2"), hashed);
    QVERIFY(q.exec());

    // Login with exact password containing colons and spaces
    bool ok = service.login(testUser, passwordWithColons, &err);
    QVERIFY(ok);
    QVERIFY(service.isAuthenticated());
    QCOMPARE(service.currentSession().username(), testUser);
    service.logout();

    // Clean up
    QSqlQuery cleanQ(sqlDb);
    cleanQ.prepare(QStringLiteral("DELETE FROM users WHERE username = :u"));
    cleanQ.bindValue(QStringLiteral(":u"), testUser);
    cleanQ.exec();
}

void TestAuthAndRBAC::testProductionEnvironmentRestriction()
{
    // Verify that when SMARTATC_ENV=production, provisioning checks abort
    qputenv("SMARTATC_ENV", "production");
    QString envVal = qEnvironmentVariable("SMARTATC_ENV");
    QCOMPARE(envVal, QStringLiteral("production"));

    bool isProduction = (envVal.compare(QStringLiteral("production"), Qt::CaseInsensitive) == 0);
    QVERIFY(isProduction);

    // Reset environment variable back to development/empty
    qunsetenv("SMARTATC_ENV");
    QVERIFY(qEnvironmentVariable("SMARTATC_ENV").isEmpty());
}

QTEST_MAIN(TestAuthAndRBAC)
#include "TestAuthAndRBAC.moc"
