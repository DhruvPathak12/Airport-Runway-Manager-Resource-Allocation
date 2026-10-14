#include "ApplicationService.h"

#include "scheduler/Scheduler.h"
#include "resource/ResourceManager.h"
#include "database/DatabaseManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include "utils/PasswordUtils.h"

ApplicationService::ApplicationService(QObject *parent)
    : QObject(parent)
    , m_scheduler(new Scheduler(this))
    , m_resourceManager(new ResourceManager(this))
{
}

bool ApplicationService::initialize()
{
    qDebug() << "[ApplicationService] Initializing application...";

    // Day 5: Attempt to load configuration and report driver availability
    bool driverAvailable = DatabaseManager::isDriverAvailable();
    if (!driverAvailable) {
        qWarning() << "[ApplicationService] QMYSQL driver plugin is not available in current Qt environment.";
    }

    // Try loading db.conf if available; if not available or invalid, continue without crashing
    bool configLoaded = DatabaseManager::instance().loadConfig();
    if (configLoaded) {
        qInfo() << "[ApplicationService] Database configuration loaded. Checking connection...";
        checkDatabaseConnection();
    } else {
        qInfo() << "[ApplicationService] Starting in offline/unconnected mode (db.conf not configured yet).";
    }

    qDebug() << "[ApplicationService] Initialization complete.";
    emit applicationReady();
    return true;
}

bool ApplicationService::checkDatabaseConnection()
{
    DatabaseManager &db = DatabaseManager::instance();
    if (db.isConnected()) {
        return true;
    }

    if (!DatabaseManager::isDriverAvailable()) {
        return false;
    }

    bool connected = db.connect();
    if (connected) {
        QString ver, curDb;
        db.testConnectivity(&ver, &curDb);
        return true;
    }

    return false;
}

bool ApplicationService::login(const QString &username, const QString &password, QString *outError)
{
    QString trimmedUser = username.trimmed();
    if (trimmedUser.isEmpty() || password.isEmpty()) {
        if (outError) {
            *outError = QStringLiteral("Username and password are required.");
        }
        return false;
    }

    DatabaseManager &dbManager = DatabaseManager::instance();
    if (!dbManager.isConnected()) {
        if (!checkDatabaseConnection()) {
            if (outError) {
                *outError = QStringLiteral("Database connection is unavailable: %1")
                                .arg(dbManager.statusDescription());
            }
            return false;
        }
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outError) {
            *outError = QStringLiteral("Database is not currently open.");
        }
        return false;
    }

    // Parameterized prepared query to look up user and role safely
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT u.user_id, u.username, u.password_hash, u.full_name, u.status, r.role_name "
        "FROM users u "
        "JOIN roles r ON u.role_id = r.role_id "
        "WHERE u.username = :username "
        "LIMIT 1;"
    ));
    query.bindValue(QStringLiteral(":username"), trimmedUser);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] User lookup query failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Authentication query failed. Please contact the administrator.");
        }
        return false;
    }

    if (!query.next()) {
        // Generic invalid credentials message to prevent username enumeration
        if (outError) {
            *outError = QStringLiteral("Invalid username or password.");
        }
        return false;
    }

    int userId = query.value(QStringLiteral("user_id")).toInt();
    QString dbUsername = query.value(QStringLiteral("username")).toString();
    QString storedHash = query.value(QStringLiteral("password_hash")).toString();
    QString fullName   = query.value(QStringLiteral("full_name")).toString();
    QString status     = query.value(QStringLiteral("status")).toString();
    QString roleName   = query.value(QStringLiteral("role_name")).toString();

    // Verify account status
    if (status.compare(QStringLiteral("Active"), Qt::CaseInsensitive) != 0) {
        if (outError) {
            *outError = QStringLiteral("Account is inactive or suspended. Please contact the administrator.");
        }
        return false;
    }

    // Verify SHA-256 password hash
    if (!PasswordUtils::verify(password, storedHash)) {
        if (outError) {
            *outError = QStringLiteral("Invalid username or password.");
        }
        return false;
    }

    // Authentication succeeded: construct in-memory session (no passwords or hashes stored)
    m_session = Session(userId, dbUsername, fullName, roleName);
    qInfo() << "[ApplicationService] User" << dbUsername << "authenticated successfully as role" << roleName;

    emit userLoggedIn(m_session);
    return true;
}

void ApplicationService::logout()
{
    if (m_session.isValid()) {
        QString loggedOutUser = m_session.username();
        m_session.clear();
        qInfo() << "[ApplicationService] User" << loggedOutUser << "logged out.";
        emit userLoggedOut();
    }
}

void ApplicationService::shutdown()
{
    qDebug() << "[ApplicationService] Shutting down...";

    // Clear active session
    logout();

    // Day 5: Disconnect cleanly from database
    DatabaseManager::instance().disconnect();

    qDebug() << "[ApplicationService] Shutdown complete.";
}

