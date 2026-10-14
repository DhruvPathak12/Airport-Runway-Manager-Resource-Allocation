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

bool ApplicationService::executeSchedulerCycle(QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) {
            *outError = QStringLiteral("Authentication required: unauthenticated users cannot trigger the scheduler.");
        }
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:ManageScheduling"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to run the scheduler.")
                            .arg(m_session.role());
        }
        return false;
    }

    if (m_scheduler) {
        m_scheduler->runScheduler();
        return true;
    }

    return false;
}

bool ApplicationService::allocateRunwayResource(int runwayId, int flightId, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) {
            *outError = QStringLiteral("Authentication required: unauthenticated users cannot allocate runways.");
        }
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:RunwayAllocation"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to allocate runways (ATC-only).")
                            .arg(m_session.role());
        }
        return false;
    }

    if (m_resourceManager) {
        return m_resourceManager->allocateRunway(runwayId, flightId);
    }

    return false;
}

bool ApplicationService::setRunwayMaintenanceStatus(int runwayId, bool inMaintenance, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) {
            *outError = QStringLiteral("Authentication required: unauthenticated users cannot alter runway status.");
        }
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:RunwayMaintenance"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to perform runway maintenance (Airport Operator-only).")
                            .arg(m_session.role());
        }
        return false;
    }

    Q_UNUSED(runwayId)
    Q_UNUSED(inMaintenance)
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Flight Management Methods (Day 7)
// ─────────────────────────────────────────────────────────────────────────────

QList<Models::Flight> ApplicationService::getAllFlights(const QString &searchFilter,
                                                       const QString &statusFilter,
                                                       QString *outError)
{
    QList<Models::Flight> results;

    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot view flights.");
        return results;
    }

    if (!m_session.hasPermission(QStringLiteral("page:Flights"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to view flights.")
                            .arg(m_session.role());
        }
        return results;
    }

    if (!checkDatabaseConnection()) {
        if (outError) {
            *outError = QStringLiteral("Database connection is offline: %1")
                            .arg(DatabaseManager::instance().statusDescription());
        }
        return results;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outError) *outError = QStringLiteral("Database is not currently open.");
        return results;
    }

    QString sql = QStringLiteral(
        "SELECT f.flight_id, f.flight_number, f.airline, f.aircraft_id, f.source, f.destination, "
        "       f.flight_type, f.scheduled_time, f.priority, f.is_emergency, f.operational_status, f.created_at, "
        "       a.registration_number, a.aircraft_type "
        "FROM flights f "
        "LEFT JOIN aircraft a ON f.aircraft_id = a.aircraft_id "
        "WHERE 1=1"
    );

    QString trimmedSearch = searchFilter.trimmed();
    bool hasSearch = !trimmedSearch.isEmpty();
    if (hasSearch) {
        sql += QStringLiteral(" AND (f.flight_number LIKE :search "
                              "     OR f.airline LIKE :search "
                              "     OR f.source LIKE :search "
                              "     OR f.destination LIKE :search "
                              "     OR a.registration_number LIKE :search)");
    }

    QString trimmedStatus = statusFilter.trimmed();
    bool hasStatus = !trimmedStatus.isEmpty() && trimmedStatus.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0;
    if (hasStatus) {
        sql += QStringLiteral(" AND f.operational_status = :status");
    }

    sql += QStringLiteral(" ORDER BY f.is_emergency DESC, f.priority ASC, f.scheduled_time ASC;");

    QSqlQuery query(db);
    query.prepare(sql);

    if (hasSearch) {
        query.bindValue(QStringLiteral(":search"), QStringLiteral("%%1%").arg(trimmedSearch));
    }
    if (hasStatus) {
        query.bindValue(QStringLiteral(":status"), trimmedStatus);
    }

    if (!query.exec()) {
        qWarning() << "[ApplicationService] getAllFlights query failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to fetch flights: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return results;
    }

    while (query.next()) {
        Models::Flight f;
        f.id = query.value(QStringLiteral("flight_id")).toInt();
        f.flightNumber = query.value(QStringLiteral("flight_number")).toString();
        f.airline = query.value(QStringLiteral("airline")).toString();
        f.aircraftId = query.value(QStringLiteral("aircraft_id")).toInt();
        f.source = query.value(QStringLiteral("source")).toString();
        f.origin = f.source;
        f.destination = query.value(QStringLiteral("destination")).toString();
        f.flightType = query.value(QStringLiteral("flight_type")).toString();
        f.scheduledTime = query.value(QStringLiteral("scheduled_time")).toDateTime();
        f.priority = query.value(QStringLiteral("priority")).toInt();
        f.isEmergency = query.value(QStringLiteral("is_emergency")).toInt() != 0;
        f.operationalStatus = query.value(QStringLiteral("operational_status")).toString();
        f.createdAt = query.value(QStringLiteral("created_at")).toDateTime();

        QString reg = query.value(QStringLiteral("registration_number")).toString();
        QString atype = query.value(QStringLiteral("aircraft_type")).toString();
        if (!reg.isEmpty()) {
            f.aircraftRegistration = QStringLiteral("%1 (%2)").arg(reg, atype);
        } else {
            f.aircraftRegistration = QStringLiteral("Unassigned");
        }

        results.append(f);
    }

    return results;
}

bool ApplicationService::getFlightById(int flightId, Models::Flight &outFlight, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot view flights.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("page:Flights"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to view flights.")
                            .arg(m_session.role());
        }
        return false;
    }

    if (!checkDatabaseConnection()) {
        if (outError) {
            *outError = QStringLiteral("Database connection is offline: %1")
                            .arg(DatabaseManager::instance().statusDescription());
        }
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outError) *outError = QStringLiteral("Database is not currently open.");
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT f.flight_id, f.flight_number, f.airline, f.aircraft_id, f.source, f.destination, "
        "       f.flight_type, f.scheduled_time, f.priority, f.is_emergency, f.operational_status, f.created_at, "
        "       a.registration_number, a.aircraft_type "
        "FROM flights f "
        "LEFT JOIN aircraft a ON f.aircraft_id = a.aircraft_id "
        "WHERE f.flight_id = :id "
        "LIMIT 1;"
    ));
    query.bindValue(QStringLiteral(":id"), flightId);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] getFlightById query failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to fetch flight: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return false;
    }

    if (!query.next()) {
        if (outError) *outError = QStringLiteral("Flight not found.");
        return false;
    }

    outFlight.id = query.value(QStringLiteral("flight_id")).toInt();
    outFlight.flightNumber = query.value(QStringLiteral("flight_number")).toString();
    outFlight.airline = query.value(QStringLiteral("airline")).toString();
    outFlight.aircraftId = query.value(QStringLiteral("aircraft_id")).toInt();
    outFlight.source = query.value(QStringLiteral("source")).toString();
    outFlight.origin = outFlight.source;
    outFlight.destination = query.value(QStringLiteral("destination")).toString();
    outFlight.flightType = query.value(QStringLiteral("flight_type")).toString();
    outFlight.scheduledTime = query.value(QStringLiteral("scheduled_time")).toDateTime();
    outFlight.priority = query.value(QStringLiteral("priority")).toInt();
    outFlight.isEmergency = query.value(QStringLiteral("is_emergency")).toInt() != 0;
    outFlight.operationalStatus = query.value(QStringLiteral("operational_status")).toString();
    outFlight.createdAt = query.value(QStringLiteral("created_at")).toDateTime();

    QString reg = query.value(QStringLiteral("registration_number")).toString();
    QString atype = query.value(QStringLiteral("aircraft_type")).toString();
    if (!reg.isEmpty()) {
        outFlight.aircraftRegistration = QStringLiteral("%1 (%2)").arg(reg, atype);
    } else {
        outFlight.aircraftRegistration = QStringLiteral("Unassigned");
    }

    return true;
}

bool ApplicationService::createFlight(const Models::Flight &flight, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot create flights.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:ManageFlights"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to create flights (ATC-only).")
                            .arg(m_session.role());
        }
        return false;
    }

    if (!flight.validate(outError)) {
        return false;
    }

    if (!checkDatabaseConnection()) {
        if (outError) {
            *outError = QStringLiteral("Database connection is offline: %1")
                            .arg(DatabaseManager::instance().statusDescription());
        }
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outError) *outError = QStringLiteral("Database is not currently open.");
        return false;
    }

    // Verify foreign key aircraft_id exists if specified
    if (flight.aircraftId > 0) {
        QSqlQuery checkAc(db);
        checkAc.prepare(QStringLiteral("SELECT COUNT(*) FROM aircraft WHERE aircraft_id = :id;"));
        checkAc.bindValue(QStringLiteral(":id"), flight.aircraftId);
        if (checkAc.exec() && checkAc.next()) {
            if (checkAc.value(0).toInt() == 0) {
                if (outError) {
                    *outError = QStringLiteral("Invalid aircraft ID %1: record does not exist.").arg(flight.aircraftId);
                }
                return false;
            }
        }
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO flights (flight_number, airline, aircraft_id, source, destination, "
        "                     flight_type, scheduled_time, priority, is_emergency, operational_status) "
        "VALUES (:flight_number, :airline, :aircraft_id, :source, :destination, "
        "        :flight_type, :scheduled_time, :priority, :is_emergency, :operational_status);"
    ));
    query.bindValue(QStringLiteral(":flight_number"), flight.flightNumber.trimmed());
    query.bindValue(QStringLiteral(":airline"), flight.airline.trimmed());
    if (flight.aircraftId > 0) {
        query.bindValue(QStringLiteral(":aircraft_id"), flight.aircraftId);
    } else {
        query.bindValue(QStringLiteral(":aircraft_id"), QVariant(QMetaType(QMetaType::Int)));
    }
    query.bindValue(QStringLiteral(":source"), flight.effectiveSource());
    query.bindValue(QStringLiteral(":destination"), flight.destination.trimmed());
    query.bindValue(QStringLiteral(":flight_type"), flight.flightType.trimmed());
    query.bindValue(QStringLiteral(":scheduled_time"), flight.scheduledTime);
    query.bindValue(QStringLiteral(":priority"), flight.priority);
    query.bindValue(QStringLiteral(":is_emergency"), flight.isEmergency ? 1 : 0);
    query.bindValue(QStringLiteral(":operational_status"), flight.operationalStatus.trimmed());

    if (!query.exec()) {
        qWarning() << "[ApplicationService] createFlight failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to insert flight: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return false;
    }

    emit flightsChanged();
    return true;
}

bool ApplicationService::updateFlight(const Models::Flight &flight, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot update flights.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:ManageFlights"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to update flights (ATC-only).")
                            .arg(m_session.role());
        }
        return false;
    }

    if (flight.id <= 0) {
        if (outError) *outError = QStringLiteral("Invalid flight ID for update.");
        return false;
    }

    if (!flight.validate(outError)) {
        return false;
    }

    if (!checkDatabaseConnection()) {
        if (outError) {
            *outError = QStringLiteral("Database connection is offline: %1")
                            .arg(DatabaseManager::instance().statusDescription());
        }
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outError) *outError = QStringLiteral("Database is not currently open.");
        return false;
    }

    // Verify foreign key aircraft_id exists if specified
    if (flight.aircraftId > 0) {
        QSqlQuery checkAc(db);
        checkAc.prepare(QStringLiteral("SELECT COUNT(*) FROM aircraft WHERE aircraft_id = :id;"));
        checkAc.bindValue(QStringLiteral(":id"), flight.aircraftId);
        if (checkAc.exec() && checkAc.next()) {
            if (checkAc.value(0).toInt() == 0) {
                if (outError) {
                    *outError = QStringLiteral("Invalid aircraft ID %1: record does not exist.").arg(flight.aircraftId);
                }
                return false;
            }
        }
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE flights SET "
        "  flight_number = :flight_number, "
        "  airline = :airline, "
        "  aircraft_id = :aircraft_id, "
        "  source = :source, "
        "  destination = :destination, "
        "  flight_type = :flight_type, "
        "  scheduled_time = :scheduled_time, "
        "  priority = :priority, "
        "  is_emergency = :is_emergency, "
        "  operational_status = :operational_status "
        "WHERE flight_id = :flight_id;"
    ));
    query.bindValue(QStringLiteral(":flight_number"), flight.flightNumber.trimmed());
    query.bindValue(QStringLiteral(":airline"), flight.airline.trimmed());
    if (flight.aircraftId > 0) {
        query.bindValue(QStringLiteral(":aircraft_id"), flight.aircraftId);
    } else {
        query.bindValue(QStringLiteral(":aircraft_id"), QVariant(QMetaType(QMetaType::Int)));
    }
    query.bindValue(QStringLiteral(":source"), flight.effectiveSource());
    query.bindValue(QStringLiteral(":destination"), flight.destination.trimmed());
    query.bindValue(QStringLiteral(":flight_type"), flight.flightType.trimmed());
    query.bindValue(QStringLiteral(":scheduled_time"), flight.scheduledTime);
    query.bindValue(QStringLiteral(":priority"), flight.priority);
    query.bindValue(QStringLiteral(":is_emergency"), flight.isEmergency ? 1 : 0);
    query.bindValue(QStringLiteral(":operational_status"), flight.operationalStatus.trimmed());
    query.bindValue(QStringLiteral(":flight_id"), flight.id);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] updateFlight failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to update flight: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return false;
    }

    if (query.numRowsAffected() == 0) {
        // Query succeeded, check if flight still exists
        QSqlQuery checkExists(db);
        checkExists.prepare(QStringLiteral("SELECT COUNT(*) FROM flights WHERE flight_id = :id;"));
        checkExists.bindValue(QStringLiteral(":id"), flight.id);
        if (checkExists.exec() && checkExists.next() && checkExists.value(0).toInt() == 0) {
            if (outError) *outError = QStringLiteral("Flight record no longer exists.");
            return false;
        }
    }

    emit flightsChanged();
    return true;
}

bool ApplicationService::canDeleteFlight(int flightId, QString *outReason)
{
    if (flightId <= 0) {
        if (outReason) *outReason = QStringLiteral("Invalid flight ID.");
        return false;
    }

    if (!checkDatabaseConnection()) {
        if (outReason) *outReason = QStringLiteral("Database connection is offline.");
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outReason) *outReason = QStringLiteral("Database is not currently open.");
        return false;
    }

    // Check runway_assignments (ON DELETE RESTRICT)
    QSqlQuery raQuery(db);
    raQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM runway_assignments WHERE flight_id = :id;"));
    raQuery.bindValue(QStringLiteral(":id"), flightId);
    if (raQuery.exec() && raQuery.next()) {
        int count = raQuery.value(0).toInt();
        if (count > 0) {
            if (outReason) {
                *outReason = QStringLiteral(
                    "Cannot delete flight: It is referenced by %1 runway assignment record(s) "
                    "with strict referential integrity (fk_ra_flight_id ON DELETE RESTRICT). "
                    "To preserve audit history, set its operational status to 'Cancelled' instead."
                ).arg(count);
            }
            return false;
        }
    }

    return true;
}

bool ApplicationService::deleteFlight(int flightId, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot delete flights.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:ManageFlights"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to delete flights (ATC-only).")
                            .arg(m_session.role());
        }
        return false;
    }

    QString blockReason;
    if (!canDeleteFlight(flightId, &blockReason)) {
        if (outError) *outError = blockReason;
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outError) *outError = QStringLiteral("Database is not currently open.");
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM flights WHERE flight_id = :id;"));
    query.bindValue(QStringLiteral(":id"), flightId);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] deleteFlight query failed:" << query.lastError().text();
        QString dbErr = query.lastError().databaseText();
        if (dbErr.contains(QStringLiteral("foreign key"), Qt::CaseInsensitive) ||
            query.lastError().nativeErrorCode() == QStringLiteral("1451")) {
            if (outError) {
                *outError = QStringLiteral(
                    "Referential integrity rejection: Cannot delete flight because related operational records reference it. "
                    "Please mark the flight as 'Cancelled' instead."
                );
            }
        } else {
            if (outError) {
                *outError = QStringLiteral("Database delete failed: %1")
                                .arg(dbErr.isEmpty() ? query.lastError().text() : dbErr);
            }
        }
        return false;
    }

    if (query.numRowsAffected() == 0) {
        if (outError) *outError = QStringLiteral("Flight not found or already deleted.");
        return false;
    }

    emit flightsChanged();
    return true;
}

QList<QPair<int, QString>> ApplicationService::getAvailableAircraft(QString *outError)
{
    QList<QPair<int, QString>> list;
    list.append(qMakePair(0, QStringLiteral("— None (Unassigned) —")));

    if (!isAuthenticated() || !m_session.hasPermission(QStringLiteral("page:Flights"))) {
        return list;
    }

    if (!checkDatabaseConnection()) {
        if (outError) *outError = QStringLiteral("Database offline.");
        return list;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        return list;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT aircraft_id, registration_number, aircraft_type, status "
        "FROM aircraft "
        "ORDER BY (status = 'Active') DESC, registration_number ASC;"
    ));

    if (query.exec()) {
        while (query.next()) {
            int aid = query.value(QStringLiteral("aircraft_id")).toInt();
            QString reg = query.value(QStringLiteral("registration_number")).toString();
            QString type = query.value(QStringLiteral("aircraft_type")).toString();
            QString status = query.value(QStringLiteral("status")).toString();
            QString label = QStringLiteral("%1 — %2 [%3]").arg(reg, type, status);
            list.append(qMakePair(aid, label));
        }
    } else {
        if (outError) {
            *outError = query.lastError().text();
        }
    }

    return list;
}


