#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "database/DatabaseManager.h"

class TestDatabaseConnectivity : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    // Configuration tests
    void testMissingConfigFile();
    void testValidConfigurationParsing();
    void testMissingRequiredHost();
    void testMissingRequiredDatabase();
    void testMissingRequiredUsername();
    void testInvalidPortRange();
    void testDefaultConfigLookupPath();

    // Driver availability test
    void testDriverAvailabilityDetection();

    // Connection lifecycle and error handling tests
    void testConnectionWithoutDriver();
    void testSanitizedErrorReporting();
    void testNoDuplicateNamedConnectionLeaks();
    void testSafeDisconnectBehavior();
    void testReadOnlyQueryWhenDisconnected();

    // Live MySQL connectivity and SELECT 1 tests
    void testLiveDatabaseConnectionAndSelectOne();
};

void TestDatabaseConnectivity::initTestCase()
{
}

void TestDatabaseConnectivity::cleanupTestCase()
{
    DatabaseManager::instance().disconnect();
}

void TestDatabaseConnectivity::cleanup()
{
    DatabaseManager::instance().disconnect();
}

void TestDatabaseConnectivity::testMissingConfigFile()
{
    DatabaseManager &db = DatabaseManager::instance();
    bool loaded = db.loadConfig(QStringLiteral("non_existent_config_file_xyz_123.conf"));
    QCOMPARE(loaded, false);
    QVERIFY(db.lastError().contains(QStringLiteral("not found")));
}

void TestDatabaseConnectivity::testValidConfigurationParsing()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QTextStream out(&tempFile);
    out << "[database]\n";
    out << "host=127.0.0.1\n";
    out << "port=3306\n";
    out << "database=smartatc\n";
    out << "username=atc_admin\n";
    out << "password=super_secret_password\n";
    tempFile.close();

    DatabaseManager &db = DatabaseManager::instance();
    bool loaded = db.loadConfig(tempFile.fileName());
    QCOMPARE(loaded, true);
    QCOMPARE(db.host(), QStringLiteral("127.0.0.1"));
    QCOMPARE(db.port(), 3306);
    QCOMPARE(db.databaseName(), QStringLiteral("smartatc"));
    QCOMPARE(db.username(), QStringLiteral("atc_admin"));
    QVERIFY(db.lastError().isEmpty());
}

void TestDatabaseConnectivity::testMissingRequiredHost()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QTextStream out(&tempFile);
    out << "[database]\n";
    out << "port=3306\n";
    out << "database=smartatc\n";
    out << "username=atc_admin\n";
    out << "password=pwd\n";
    tempFile.close();

    DatabaseManager &db = DatabaseManager::instance();
    bool loaded = db.loadConfig(tempFile.fileName());
    QCOMPARE(loaded, false);
    QVERIFY(db.lastError().contains(QStringLiteral("'host' is missing")));
}

void TestDatabaseConnectivity::testMissingRequiredDatabase()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QTextStream out(&tempFile);
    out << "[database]\n";
    out << "host=127.0.0.1\n";
    out << "port=3306\n";
    out << "username=atc_admin\n";
    out << "password=pwd\n";
    tempFile.close();

    DatabaseManager &db = DatabaseManager::instance();
    bool loaded = db.loadConfig(tempFile.fileName());
    QCOMPARE(loaded, false);
    QVERIFY(db.lastError().contains(QStringLiteral("'database' name is missing")));
}

void TestDatabaseConnectivity::testMissingRequiredUsername()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QTextStream out(&tempFile);
    out << "[database]\n";
    out << "host=127.0.0.1\n";
    out << "port=3306\n";
    out << "database=smartatc\n";
    out << "password=pwd\n";
    tempFile.close();

    DatabaseManager &db = DatabaseManager::instance();
    bool loaded = db.loadConfig(tempFile.fileName());
    QCOMPARE(loaded, false);
    QVERIFY(db.lastError().contains(QStringLiteral("'username' is missing")));
}

void TestDatabaseConnectivity::testInvalidPortRange()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QTextStream out(&tempFile);
    out << "[database]\n";
    out << "host=127.0.0.1\n";
    out << "port=999999\n";
    out << "database=smartatc\n";
    out << "username=atc_admin\n";
    tempFile.close();

    DatabaseManager &db = DatabaseManager::instance();
    bool loaded = db.loadConfig(tempFile.fileName());
    QCOMPARE(loaded, false);
    QVERIFY(db.lastError().contains(QStringLiteral("invalid 'port'")));
}

void TestDatabaseConnectivity::testDefaultConfigLookupPath()
{
    QString defPath = DatabaseManager::defaultConfigFile();
    QVERIFY(!defPath.isEmpty());
    QVERIFY(defPath.endsWith(QStringLiteral("db.conf")));
}

void TestDatabaseConnectivity::testDriverAvailabilityDetection()
{
    // Verifies whether QMYSQL is detected accurately
    bool hasDriver = DatabaseManager::isDriverAvailable();
    bool expectedAvailability = QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL"));
    QCOMPARE(hasDriver, expectedAvailability);
}

void TestDatabaseConnectivity::testConnectionWithoutDriver()
{
    // Setup config
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QTextStream out(&tempFile);
    out << "[database]\n";
    out << "host=127.0.0.1\n";
    out << "port=3306\n";
    out << "database=smartatc\n";
    out << "username=testuser\n";
    out << "password=testpass\n";
    tempFile.close();

    DatabaseManager &db = DatabaseManager::instance();
    QVERIFY(db.loadConfig(tempFile.fileName()));

    if (!DatabaseManager::isDriverAvailable()) {
        bool connected = db.connect();
        QCOMPARE(connected, false);
        QCOMPARE(db.isConnected(), false);
        QVERIFY(db.lastError().contains(QStringLiteral("QMYSQL")));
    }
}

void TestDatabaseConnectivity::testSanitizedErrorReporting()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QString secretPassword = QStringLiteral("P@ssw0rdSecret987!");
    QTextStream out(&tempFile);
    out << "[database]\n";
    out << "host=127.0.0.1\n";
    out << "port=3306\n";
    out << "database=smartatc\n";
    out << "username=testuser\n";
    out << "password=" << secretPassword << "\n";
    tempFile.close();

    DatabaseManager &db = DatabaseManager::instance();
    db.loadConfig(tempFile.fileName());
    db.connect();

    QString err = db.lastError();
    // Verify password is never leaked in error
    QVERIFY(!err.contains(secretPassword));
}

void TestDatabaseConnectivity::testNoDuplicateNamedConnectionLeaks()
{
    DatabaseManager &db = DatabaseManager::instance();
    // Call connect multiple times
    db.connect();
    db.connect();

    // Verify named connection is clean and not duplicated or corrupted
    if (QSqlDatabase::contains(DatabaseManager::CONNECTION_NAME)) {
        QSqlDatabase conn = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME, false);
        QVERIFY(conn.isValid());
    }
    db.disconnect();
    QCOMPARE(QSqlDatabase::contains(DatabaseManager::CONNECTION_NAME), false);
}

void TestDatabaseConnectivity::testSafeDisconnectBehavior()
{
    DatabaseManager &db = DatabaseManager::instance();
    db.disconnect();
    QCOMPARE(db.isConnected(), false);
    QCOMPARE(QSqlDatabase::contains(DatabaseManager::CONNECTION_NAME), false);
}

void TestDatabaseConnectivity::testReadOnlyQueryWhenDisconnected()
{
    DatabaseManager &db = DatabaseManager::instance();
    db.disconnect();

    QString version, database;
    bool success = db.testConnectivity(&version, &database);
    QCOMPARE(success, false);
    QCOMPARE(db.lastError(), QStringLiteral("Not connected to database."));
}

void TestDatabaseConnectivity::testLiveDatabaseConnectionAndSelectOne()
{
    DatabaseManager &db = DatabaseManager::instance();
    QString defaultCfg = DatabaseManager::defaultConfigFile();

    if (!QFile::exists(defaultCfg)) {
        QSKIP("Local db.conf is not configured on this machine; skipping live MySQL test.");
    }

    bool loaded = db.loadConfig(defaultCfg);
    QVERIFY(loaded);

    bool connected = db.connect();
    if (!connected) {
        QSKIP(qPrintable(QStringLiteral("Could not connect to live MySQL with db.conf (%1); skipping live query test.")
                             .arg(db.lastError())));
    }

    QVERIFY(db.isConnected());
    QCOMPARE(db.connectionState(), DatabaseManager::ConnectionState::Connected);

    // Verify SELECT 1 returns 1 within local scope to free handle before disconnect
    {
        QSqlDatabase sqlDb = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        QSqlQuery query(sqlDb);
        QVERIFY(query.exec(QStringLiteral("SELECT 1;")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
    }

    // Verify active database matches expected database
    QString version, currentDb;
    QVERIFY(db.testConnectivity(&version, &currentDb));
    QVERIFY(!version.isEmpty());
    QCOMPARE(currentDb.toLower(), QStringLiteral("smartatc"));

    db.disconnect();
    QCOMPARE(db.isConnected(), false);
}

QTEST_MAIN(TestDatabaseConnectivity)
#include "TestDatabaseConnectivity.moc"
