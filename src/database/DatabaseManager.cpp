#include "DatabaseManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

const QString DatabaseManager::CONNECTION_NAME = QStringLiteral("SmartATCConnection");

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager s_instance;
    return s_instance;
}

DatabaseManager::DatabaseManager()
    : m_connected(false)
{
}

DatabaseManager::~DatabaseManager()
{
    disconnect();
}

// ─────────────────────────────────────────────────────────────────────────────
// Configuration and Driver Inspection
// ─────────────────────────────────────────────────────────────────────────────

QString DatabaseManager::defaultConfigFile()
{
    // Search candidates deterministically:
    // 1. Current working directory: ./db.conf
    // 2. Application executable directory: app_dir/db.conf
    // 3. Parent directory of application executable (e.g. project root when running from build/): app_dir/../db.conf
    QStringList candidates;
    candidates << QDir::current().filePath(QStringLiteral("db.conf"));

    QString appDirPath = QCoreApplication::applicationDirPath();
    if (!appDirPath.isEmpty()) {
        candidates << QDir(appDirPath).filePath(QStringLiteral("db.conf"));
        candidates << QDir(appDirPath).filePath(QStringLiteral("../db.conf"));
    }

    for (const QString &path : candidates) {
        QFileInfo fi(path);
        if (fi.exists() && fi.isFile()) {
            return fi.canonicalFilePath();
        }
    }

    // Default fallback path for reporting when file does not exist
    return QDir::current().filePath(QStringLiteral("db.conf"));
}

bool DatabaseManager::isDriverAvailable()
{
    return QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL"));
}

bool DatabaseManager::loadConfig(const QString &configFilePath)
{
    QString resolvedPath = configFilePath.trimmed();
    if (resolvedPath.isEmpty()) {
        resolvedPath = defaultConfigFile();
    }

    QFileInfo fi(resolvedPath);
    if (!fi.exists() || !fi.isFile()) {
        m_lastError = QStringLiteral("Database configuration file not found: %1").arg(resolvedPath);
        qWarning() << "[DatabaseManager]" << m_lastError;
        return false;
    }

    m_configFilePath = fi.canonicalFilePath();

    // Parse INI file. QSettings supports INI natively.
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    QString host = settings.value(QStringLiteral("database/host"),
                   settings.value(QStringLiteral("host"))).toString().trimmed();

    QString portStr = settings.value(QStringLiteral("database/port"),
                      settings.value(QStringLiteral("port"), QStringLiteral("3306"))).toString().trimmed();

    QString dbName = settings.value(QStringLiteral("database/database"),
                     settings.value(QStringLiteral("database"),
                     settings.value(QStringLiteral("database/dbname"),
                     settings.value(QStringLiteral("dbname"))))).toString().trimmed();

    QString username = settings.value(QStringLiteral("database/username"),
                       settings.value(QStringLiteral("username"),
                       settings.value(QStringLiteral("database/user"),
                       settings.value(QStringLiteral("user"))))).toString().trimmed();

    QString password = settings.value(QStringLiteral("database/password"),
                       settings.value(QStringLiteral("password"))).toString();

    // Validation
    if (host.isEmpty()) {
        m_lastError = QStringLiteral("Configuration error: 'host' is missing in %1").arg(m_configFilePath);
        qWarning() << "[DatabaseManager]" << m_lastError;
        return false;
    }

    bool portOk = false;
    int port = portStr.toInt(&portOk);
    if (!portOk || port <= 0 || port > 65535) {
        m_lastError = QStringLiteral("Configuration error: invalid 'port' (%1) in %2").arg(portStr, m_configFilePath);
        qWarning() << "[DatabaseManager]" << m_lastError;
        return false;
    }

    if (dbName.isEmpty()) {
        m_lastError = QStringLiteral("Configuration error: 'database' name is missing in %1").arg(m_configFilePath);
        qWarning() << "[DatabaseManager]" << m_lastError;
        return false;
    }

    if (username.isEmpty()) {
        m_lastError = QStringLiteral("Configuration error: 'username' is missing in %1").arg(m_configFilePath);
        qWarning() << "[DatabaseManager]" << m_lastError;
        return false;
    }

    // Do not substitute insecure fallback credentials
    m_config.clear();
    m_config.insert(QStringLiteral("host"), host);
    m_config.insert(QStringLiteral("port"), QString::number(port));
    m_config.insert(QStringLiteral("database"), dbName);
    m_config.insert(QStringLiteral("username"), username);
    m_config.insert(QStringLiteral("password"), password);

    m_lastError.clear();
    qInfo() << "[DatabaseManager] Configuration loaded successfully from" << m_configFilePath
            << "for host:" << host << "port:" << port << "database:" << dbName
            << "user:" << username;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Connection Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

bool DatabaseManager::connect()
{
    // Disconnect any active connection first
    if (m_connected) {
        disconnect();
    }

    // 1. Verify that configuration is loaded
    if (m_config.isEmpty() || !m_config.contains(QStringLiteral("host"))) {
        // Attempt loading default config if not yet loaded
        if (!loadConfig()) {
            m_state = ConnectionState::NotConfigured;
            setSanitizedError(QStringLiteral("Cannot connect: configuration is missing or invalid."));
            return false;
        }
    }

    // 2. Check QMYSQL driver availability
    if (!isDriverAvailable()) {
        m_state = ConnectionState::DriverUnavailable;
        setSanitizedError(QStringLiteral("Qt SQL 'QMYSQL' driver plugin is not available in current Qt environment."));
        qCritical() << "[DatabaseManager]" << m_lastError;
        return false;
    }

    // 3. Qt SQL connection setup — ensure no duplicate named connection leak
    QSqlDatabase db;
    if (QSqlDatabase::contains(CONNECTION_NAME)) {
        db = QSqlDatabase::database(CONNECTION_NAME);
    } else {
        db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), CONNECTION_NAME);
    }

    db.setHostName(m_config.value(QStringLiteral("host")));
    db.setPort(m_config.value(QStringLiteral("port")).toInt());
    db.setDatabaseName(m_config.value(QStringLiteral("database")));
    db.setUserName(m_config.value(QStringLiteral("username")));
    db.setPassword(m_config.value(QStringLiteral("password")));

    // 4. Attempt opening connection
    if (!db.open()) {
        QString errText = db.lastError().text();
        setSanitizedError(errText);

        // Distinguish specific failure modes
        if (errText.contains(QStringLiteral("Access denied"), Qt::CaseInsensitive)) {
            m_state = ConnectionState::AuthFailed;
        } else if (errText.contains(QStringLiteral("Unknown database"), Qt::CaseInsensitive)) {
            m_state = ConnectionState::DatabaseNotFound;
        } else if (errText.contains(QStringLiteral("Can't connect"), Qt::CaseInsensitive) ||
                   errText.contains(QStringLiteral("Communications link failure"), Qt::CaseInsensitive)) {
            m_state = ConnectionState::ServerUnreachable;
        } else {
            m_state = ConnectionState::Disconnected;
        }

        qWarning() << "[DatabaseManager] Failed to connect to MySQL database:" << m_lastError;
        m_connected = false;
        return false;
    }

    m_connected = true;
    m_state = ConnectionState::Connected;
    m_lastError.clear();
    qInfo() << "[DatabaseManager] Connected successfully to MySQL database"
            << db.databaseName() << "on" << db.hostName();
    return true;
}

void DatabaseManager::disconnect()
{
    if (QSqlDatabase::contains(CONNECTION_NAME)) {
        {
            // Close database within a local scope so handle is released before removal
            QSqlDatabase db = QSqlDatabase::database(CONNECTION_NAME, false);
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(CONNECTION_NAME);
    }
    m_connected = false;
    m_state = ConnectionState::Disconnected;
    qInfo() << "[DatabaseManager] Database connection closed.";
}

DatabaseManager::ConnectionState DatabaseManager::connectionState() const
{
    return m_state;
}

QString DatabaseManager::statusDescription() const
{
    switch (m_state) {
    case ConnectionState::Connected:
        return QStringLiteral("Connected (Database: %1, Host: %2:%3)")
            .arg(databaseName(), host(), QString::number(port()));
    case ConnectionState::NotConfigured:
        return QStringLiteral("Configuration missing (db.conf not found or incomplete)");
    case ConnectionState::DriverUnavailable:
        return QStringLiteral("QMYSQL driver plugin unavailable");
    case ConnectionState::ServerUnreachable:
        return QStringLiteral("MySQL server unreachable on %1:%2").arg(host(), QString::number(port()));
    case ConnectionState::AuthFailed:
        return QStringLiteral("Authentication failed for user '%1'").arg(username());
    case ConnectionState::DatabaseNotFound:
        return QStringLiteral("Database '%1' not found on server").arg(databaseName());
    case ConnectionState::Disconnected:
    default:
        return QStringLiteral("Disconnected");
    }
}

bool DatabaseManager::isConnected() const
{
    if (!m_connected) {
        return false;
    }
    if (!QSqlDatabase::contains(CONNECTION_NAME)) {
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(CONNECTION_NAME, false);
    return db.isOpen();
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

// ─────────────────────────────────────────────────────────────────────────────
// Read-Only Connectivity Check
// ─────────────────────────────────────────────────────────────────────────────

bool DatabaseManager::testConnectivity(QString *outVersion, QString *outDatabase)
{
    if (!isConnected()) {
        setSanitizedError(QStringLiteral("Not connected to database."));
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(CONNECTION_NAME);
    QSqlQuery query(db);

    // Minimal, strictly read-only query
    if (!query.exec(QStringLiteral("SELECT VERSION(), DATABASE();"))) {
        setSanitizedError(query.lastError().text());
        qWarning() << "[DatabaseManager] Test query execution failed:" << m_lastError;
        return false;
    }

    if (query.next()) {
        QString ver = query.value(0).toString();
        QString curDb = query.value(1).toString();

        if (outVersion) {
            *outVersion = ver;
        }
        if (outDatabase) {
            *outDatabase = curDb;
        }

        qInfo() << "[DatabaseManager] Read-only connectivity test succeeded. Server Version:"
                << ver << "Active Database:" << curDb;
        return true;
    }

    setSanitizedError(QStringLiteral("Test query returned no records."));
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Accessors and Helpers
// ─────────────────────────────────────────────────────────────────────────────

QString DatabaseManager::host() const
{
    return m_config.value(QStringLiteral("host"));
}

int DatabaseManager::port() const
{
    return m_config.value(QStringLiteral("port")).toInt();
}

QString DatabaseManager::databaseName() const
{
    return m_config.value(QStringLiteral("database"));
}

QString DatabaseManager::username() const
{
    return m_config.value(QStringLiteral("username"));
}

void DatabaseManager::setSanitizedError(const QString &rawError)
{
    QString sanitized = rawError.trimmed();

    // Strip password if it accidentally leaked into any error string
    QString pwd = m_config.value(QStringLiteral("password"));
    if (!pwd.isEmpty() && sanitized.contains(pwd)) {
        sanitized.replace(pwd, QStringLiteral("******"));
    }

    m_lastError = sanitized;
}

