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
    return allocateRunway(runwayId, flightId, outError) == Models::AllocationResult::Success;
}

Models::AllocationResult ApplicationService::allocateRunway(int runwayId, int flightId, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) {
            *outError = QStringLiteral("Authentication required: unauthenticated users cannot allocate runways.");
        }
        return Models::AllocationResult::Unauthorized;
    }

    if (!m_session.hasPermission(QStringLiteral("action:RunwayAllocation"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to allocate runways (ATC-only).")
                            .arg(m_session.role());
        }
        return Models::AllocationResult::Unauthorized;
    }

    if (runwayId <= 0 || flightId <= 0) {
        if (outError) {
            *outError = QStringLiteral("Invalid input parameters: runway ID (%1) and flight ID (%2) must be positive integers.")
                            .arg(runwayId).arg(flightId);
        }
        return Models::AllocationResult::InvalidInput;
    }

    if (!checkDatabaseConnection()) {
        if (outError) {
            *outError = QStringLiteral("Database connection is offline: %1")
                            .arg(DatabaseManager::instance().statusDescription());
        }
        return Models::AllocationResult::DatabaseError;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outError) {
            *outError = QStringLiteral("Database is not currently open.");
        }
        return Models::AllocationResult::DatabaseError;
    }

    // ── 1. Verify Runway existence, type, and availability ───────────────────
    QString runwayNumber;
    QString runwayType;
    QString runwayStatus;
    {
        QSqlQuery rwQuery(db);
        rwQuery.prepare(QStringLiteral("SELECT runway_number, runway_type, status FROM runways WHERE runway_id = :id;"));
        rwQuery.bindValue(QStringLiteral(":id"), runwayId);
        if (!rwQuery.exec()) {
            qWarning() << "[ApplicationService] allocateRunway: runway check failed:" << rwQuery.lastError().text();
            if (outError) {
                *outError = QStringLiteral("Failed to query runway state: %1").arg(rwQuery.lastError().text());
            }
            return Models::AllocationResult::DatabaseError;
        }

        if (!rwQuery.next()) {
            if (outError) {
                *outError = QStringLiteral("Runway with ID %1 was not found in the database.").arg(runwayId);
            }
            return Models::AllocationResult::RunwayNotFound;
        }

        runwayNumber = rwQuery.value(0).toString();
        runwayType   = rwQuery.value(1).toString();
        runwayStatus = rwQuery.value(2).toString();

        if (runwayStatus.compare(QStringLiteral("Available"), Qt::CaseInsensitive) != 0) {
            if (outError) {
                *outError = QStringLiteral("Runway '%1' is currently '%2' and cannot be allocated. Runways must be Available.")
                                .arg(runwayNumber, runwayStatus);
            }
            return Models::AllocationResult::RunwayUnavailable;
        }
    }

    // ── 2. Reject second active assignment to the same runway ───────────────
    {
        QSqlQuery rwAssignQuery(db);
        rwAssignQuery.prepare(QStringLiteral(
            "SELECT ra.assignment_id, f.flight_number "
            "FROM runway_assignments ra "
            "LEFT JOIN flights f ON ra.flight_id = f.flight_id "
            "WHERE ra.runway_id = :rid AND ra.status = 'Active';"
        ));
        rwAssignQuery.bindValue(QStringLiteral(":rid"), runwayId);
        if (!rwAssignQuery.exec()) {
            qWarning() << "[ApplicationService] allocateRunway: runway active assignment check failed:" << rwAssignQuery.lastError().text();
            if (outError) {
                *outError = QStringLiteral("Failed to check active runway assignments: %1").arg(rwAssignQuery.lastError().text());
            }
            return Models::AllocationResult::DatabaseError;
        }

        if (rwAssignQuery.next()) {
            int activeAssignId = rwAssignQuery.value(0).toInt();
            QString assignedFlightNum = rwAssignQuery.value(1).toString();
            if (outError) {
                *outError = QStringLiteral("Runway '%1' already has an active assignment (Assignment #%2%3).")
                                .arg(runwayNumber)
                                .arg(activeAssignId)
                                .arg(assignedFlightNum.isEmpty() ? QString() : QStringLiteral(" for flight %1").arg(assignedFlightNum));
            }
            return Models::AllocationResult::ConflictExistingAssignment;
        }
    }

    // ── 3. Verify Flight existence, operational status, and type compatibility
    QString flightNumber;
    QString flightType;
    QString flightStatus;
    {
        QSqlQuery fQuery(db);
        fQuery.prepare(QStringLiteral(
            "SELECT flight_number, flight_type, operational_status FROM flights WHERE flight_id = :fid;"
        ));
        fQuery.bindValue(QStringLiteral(":fid"), flightId);
        if (!fQuery.exec()) {
            qWarning() << "[ApplicationService] allocateRunway: flight check failed:" << fQuery.lastError().text();
            if (outError) {
                *outError = QStringLiteral("Failed to query flight state: %1").arg(fQuery.lastError().text());
            }
            return Models::AllocationResult::DatabaseError;
        }

        if (!fQuery.next()) {
            if (outError) {
                *outError = QStringLiteral("Flight with ID %1 was not found in the database.").arg(flightId);
            }
            return Models::AllocationResult::FlightNotFound;
        }

        flightNumber = fQuery.value(0).toString();
        flightType   = fQuery.value(1).toString();
        flightStatus = fQuery.value(2).toString();

        // Disallow terminal flights: Landed, Departed, Cancelled
        if (flightStatus.compare(QStringLiteral("Landed"), Qt::CaseInsensitive) == 0 ||
            flightStatus.compare(QStringLiteral("Departed"), Qt::CaseInsensitive) == 0 ||
            flightStatus.compare(QStringLiteral("Cancelled"), Qt::CaseInsensitive) == 0) {
            if (outError) {
                *outError = QStringLiteral("Flight '%1' is in terminal status '%2' and cannot be allocated to a runway.")
                                .arg(flightNumber, flightStatus);
            }
            return Models::AllocationResult::FlightIneligible;
        }

        // Validate runway type operational compatibility
        if (runwayType.compare(QStringLiteral("Landing"), Qt::CaseInsensitive) == 0 &&
            flightType.compare(QStringLiteral("Departure"), Qt::CaseInsensitive) == 0) {
            if (outError) {
                *outError = QStringLiteral("Runway '%1' is designated for Landing only, but flight '%2' is a Departure.")
                                .arg(runwayNumber, flightNumber);
            }
            return Models::AllocationResult::FlightIneligible;
        }

        if (runwayType.compare(QStringLiteral("Takeoff"), Qt::CaseInsensitive) == 0 &&
            flightType.compare(QStringLiteral("Arrival"), Qt::CaseInsensitive) == 0) {
            if (outError) {
                *outError = QStringLiteral("Runway '%1' is designated for Takeoff only, but flight '%2' is an Arrival.")
                                .arg(runwayNumber, flightNumber);
            }
            return Models::AllocationResult::FlightIneligible;
        }
    }

    // ── 4. Reject duplicate active assignment for the same flight ───────────
    {
        QSqlQuery fAssignQuery(db);
        fAssignQuery.prepare(QStringLiteral(
            "SELECT ra.assignment_id, r.runway_number "
            "FROM runway_assignments ra "
            "LEFT JOIN runways r ON ra.runway_id = r.runway_id "
            "WHERE ra.flight_id = :fid AND ra.status = 'Active';"
        ));
        fAssignQuery.bindValue(QStringLiteral(":fid"), flightId);
        if (!fAssignQuery.exec()) {
            qWarning() << "[ApplicationService] allocateRunway: flight active assignment check failed:" << fAssignQuery.lastError().text();
            if (outError) {
                *outError = QStringLiteral("Failed to check existing flight assignment: %1").arg(fAssignQuery.lastError().text());
            }
            return Models::AllocationResult::DatabaseError;
        }

        if (fAssignQuery.next()) {
            int activeAssignId = fAssignQuery.value(0).toInt();
            QString activeRunwayNum = fAssignQuery.value(1).toString();
            if (outError) {
                *outError = QStringLiteral("Flight '%1' already possesses an active assignment (Assignment #%2 on Runway %3).")
                                .arg(flightNumber)
                                .arg(activeAssignId)
                                .arg(activeRunwayNum.isEmpty() ? QStringLiteral("?") : activeRunwayNum);
            }
            return Models::AllocationResult::ConflictExistingAssignment;
        }
    }

    // ── 5. Verify process simulation state in scheduling_queue if present ───
    {
        QSqlQuery qQuery(db);
        qQuery.prepare(QStringLiteral("SELECT process_state FROM scheduling_queue WHERE flight_id = :fid;"));
        qQuery.bindValue(QStringLiteral(":fid"), flightId);
        if (qQuery.exec() && qQuery.next()) {
            QString procState = qQuery.value(0).toString();
            if (procState.compare(QStringLiteral("Completed"), Qt::CaseInsensitive) == 0) {
                if (outError) {
                    *outError = QStringLiteral("Flight process for '%1' is in Completed state and cannot be allocated.")
                                    .arg(flightNumber);
                }
                return Models::AllocationResult::FlightIneligible;
            }
        }
    }

    // ── 6. Persist Assignment and Update States ──────────────────────────────
    // 6a. Insert into runway_assignments
    QVariant assignedByVal(QMetaType(QMetaType::Int));
    if (m_session.userId() > 0) {
        QSqlQuery userCheck(db);
        userCheck.prepare(QStringLiteral("SELECT 1 FROM users WHERE user_id = :uid;"));
        userCheck.bindValue(QStringLiteral(":uid"), m_session.userId());
        if (userCheck.exec() && userCheck.next()) {
            assignedByVal = m_session.userId();
        }
    }

    QSqlQuery insertQuery(db);
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO runway_assignments (flight_id, runway_id, assigned_by, assigned_at, status) "
        "VALUES (:fid, :rid, :uid, NOW(), 'Active');"
    ));
    insertQuery.bindValue(QStringLiteral(":fid"), flightId);
    insertQuery.bindValue(QStringLiteral(":rid"), runwayId);
    insertQuery.bindValue(QStringLiteral(":uid"), assignedByVal);

    if (!insertQuery.exec()) {
        qWarning() << "[ApplicationService] allocateRunway: INSERT runway_assignments failed:" << insertQuery.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to insert runway assignment record: %1")
                            .arg(insertQuery.lastError().text());
        }
        return Models::AllocationResult::DatabaseError;
    }

    // 6b. Update runways status to 'Occupied'
    QSqlQuery updateRwQuery(db);
    updateRwQuery.prepare(QStringLiteral("UPDATE runways SET status = 'Occupied' WHERE runway_id = :rid;"));
    updateRwQuery.bindValue(QStringLiteral(":rid"), runwayId);
    if (!updateRwQuery.exec()) {
        qWarning() << "[ApplicationService] allocateRunway: UPDATE runways status failed:" << updateRwQuery.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to update runway occupancy status: %1")
                            .arg(updateRwQuery.lastError().text());
        }
        return Models::AllocationResult::DatabaseError;
    }

    // 6c. Advance flight operational_status from Scheduled/Taxiing to Cleared
    if (flightStatus.compare(QStringLiteral("Scheduled"), Qt::CaseInsensitive) == 0 ||
        flightStatus.compare(QStringLiteral("Taxiing"), Qt::CaseInsensitive) == 0) {
        QSqlQuery updateFlightQuery(db);
        updateFlightQuery.prepare(QStringLiteral(
            "UPDATE flights SET operational_status = 'Cleared' WHERE flight_id = :fid;"
        ));
        updateFlightQuery.bindValue(QStringLiteral(":fid"), flightId);
        if (!updateFlightQuery.exec()) {
            qWarning() << "[ApplicationService] allocateRunway: UPDATE flight status warning:" << updateFlightQuery.lastError().text();
        }
    }

    // 6d. Insert audit log record into operation_logs
    QSqlQuery logQuery(db);
    logQuery.prepare(QStringLiteral(
        "INSERT INTO operation_logs (user_id, operation_type, flight_id, runway_id, transaction_status, message, created_at) "
        "VALUES (:uid, 'RUNWAY_ASSIGN', :fid, :rid, 'N/A', :msg, NOW());"
    ));
    logQuery.bindValue(QStringLiteral(":uid"), assignedByVal);
    logQuery.bindValue(QStringLiteral(":fid"), flightId);
    logQuery.bindValue(QStringLiteral(":rid"), runwayId);
    logQuery.bindValue(QStringLiteral(":msg"), QStringLiteral("Runway %1 allocated to flight %2 by %3.")
                                                  .arg(runwayNumber, flightNumber, m_session.username()));
    if (!logQuery.exec()) {
        qWarning() << "[ApplicationService] allocateRunway: operation_logs warning:" << logQuery.lastError().text();
    }

    // ── 7. Notify UI and Subsystems ─────────────────────────────────────────
    if (m_resourceManager) {
        emit m_resourceManager->runwayStatusChanged(runwayId, QStringLiteral("Occupied"));
    }

    emit runwaysChanged();
    emit flightsChanged();
    emit processesChanged();

    return Models::AllocationResult::Success;
}

bool ApplicationService::releaseRunway(int runwayId, QString *outError)
{
    return releaseRunwayResource(runwayId, outError);
}

bool ApplicationService::releaseRunwayResource(int runwayId, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) {
            *outError = QStringLiteral("Authentication required: unauthenticated users cannot release runways.");
        }
        return false;
    }

    // Releasing a runway is permitted for ATC (controllers), Airport Operator (maintenance/ops), and Admin
    bool canRelease = m_session.hasPermission(QStringLiteral("action:RunwayAllocation")) ||
                      m_session.hasPermission(QStringLiteral("action:RunwayMaintenance")) ||
                      m_session.isAdmin();

    if (!canRelease) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to release runways.")
                            .arg(m_session.role());
        }
        return false;
    }

    if (runwayId <= 0) {
        if (outError) {
            *outError = QStringLiteral("Invalid runway ID: %1. Must be greater than zero.").arg(runwayId);
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
        if (outError) {
            *outError = QStringLiteral("Database is not currently open.");
        }
        return false;
    }

    // Verify runway exists
    QString runwayNumber;
    QString currentStatus;
    {
        QSqlQuery rwQuery(db);
        rwQuery.prepare(QStringLiteral("SELECT runway_number, status FROM runways WHERE runway_id = :id;"));
        rwQuery.bindValue(QStringLiteral(":id"), runwayId);
        if (!rwQuery.exec() || !rwQuery.next()) {
            if (outError) {
                *outError = QStringLiteral("Runway with ID %1 was not found.").arg(runwayId);
            }
            return false;
        }
        runwayNumber  = rwQuery.value(0).toString();
        currentStatus = rwQuery.value(1).toString();

        if (currentStatus.compare(QStringLiteral("Maintenance"), Qt::CaseInsensitive) == 0) {
            if (outError) {
                *outError = QStringLiteral("Runway '%1' is currently in Maintenance. Return to Available via maintenance toggle.")
                                .arg(runwayNumber);
            }
            return false;
        }
    }

    // Find active assignment
    int activeAssignmentId = 0;
    int assignedFlightId = 0;
    QString flightNumber;
    {
        QSqlQuery raQuery(db);
        raQuery.prepare(QStringLiteral(
            "SELECT ra.assignment_id, ra.flight_id, f.flight_number "
            "FROM runway_assignments ra "
            "LEFT JOIN flights f ON ra.flight_id = f.flight_id "
            "WHERE ra.runway_id = :rid AND ra.status = 'Active';"
        ));
        raQuery.bindValue(QStringLiteral(":rid"), runwayId);
        if (raQuery.exec() && raQuery.next()) {
            activeAssignmentId = raQuery.value(0).toInt();
            assignedFlightId   = raQuery.value(1).toInt();
            flightNumber       = raQuery.value(2).toString();
        }
    }

    // Update active assignment to 'Released'
    if (activeAssignmentId > 0) {
        QSqlQuery updateRaQuery(db);
        updateRaQuery.prepare(QStringLiteral(
            "UPDATE runway_assignments SET status = 'Released', released_at = NOW() WHERE assignment_id = :aid;"
        ));
        updateRaQuery.bindValue(QStringLiteral(":aid"), activeAssignmentId);
        if (!updateRaQuery.exec()) {
            qWarning() << "[ApplicationService] releaseRunway: UPDATE runway_assignments failed:" << updateRaQuery.lastError().text();
            if (outError) {
                *outError = QStringLiteral("Failed to update runway assignment state: %1").arg(updateRaQuery.lastError().text());
            }
            return false;
        }
    }

    // Update runway status to 'Available'
    QSqlQuery updateRwQuery(db);
    updateRwQuery.prepare(QStringLiteral("UPDATE runways SET status = 'Available' WHERE runway_id = :rid;"));
    updateRwQuery.bindValue(QStringLiteral(":rid"), runwayId);
    if (!updateRwQuery.exec()) {
        qWarning() << "[ApplicationService] releaseRunway: UPDATE runways status failed:" << updateRwQuery.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to update runway status to Available: %1").arg(updateRwQuery.lastError().text());
        }
        return false;
    }

    // Audit log
    QVariant releaseUserVal(QMetaType(QMetaType::Int));
    if (m_session.userId() > 0) {
        QSqlQuery userCheck(db);
        userCheck.prepare(QStringLiteral("SELECT 1 FROM users WHERE user_id = :uid;"));
        userCheck.bindValue(QStringLiteral(":uid"), m_session.userId());
        if (userCheck.exec() && userCheck.next()) {
            releaseUserVal = m_session.userId();
        }
    }

    QSqlQuery logQuery(db);
    logQuery.prepare(QStringLiteral(
        "INSERT INTO operation_logs (user_id, operation_type, flight_id, runway_id, transaction_status, message, created_at) "
        "VALUES (:uid, 'RUNWAY_RELEASE', :fid, :rid, 'N/A', :msg, NOW());"
    ));
    logQuery.bindValue(QStringLiteral(":uid"), releaseUserVal);
    if (assignedFlightId > 0) {
        logQuery.bindValue(QStringLiteral(":fid"), assignedFlightId);
    } else {
        logQuery.bindValue(QStringLiteral(":fid"), QVariant(QMetaType(QMetaType::Int)));
    }
    logQuery.bindValue(QStringLiteral(":rid"), runwayId);
    logQuery.bindValue(QStringLiteral(":msg"), QStringLiteral("Runway %1 released%2 by %3.")
                                                  .arg(runwayNumber)
                                                  .arg(flightNumber.isEmpty() ? QString() : QStringLiteral(" from flight %1").arg(flightNumber))
                                                  .arg(m_session.username()));
    if (!logQuery.exec()) {
        qWarning() << "[ApplicationService] releaseRunway: operation_logs warning:" << logQuery.lastError().text();
    }

    if (m_resourceManager) {
        emit m_resourceManager->runwayStatusChanged(runwayId, QStringLiteral("Available"));
    }

    emit runwaysChanged();
    emit flightsChanged();
    emit processesChanged();

    return true;
}

QList<Models::Flight> ApplicationService::getEligibleFlightsForRunwayAllocation(int runwayId, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) {
            *outError = QStringLiteral("Authentication required: unauthenticated users cannot view eligible flights.");
        }
        return {};
    }

    if (!checkDatabaseConnection()) {
        if (outError) {
            *outError = QStringLiteral("Database connection is offline: %1")
                            .arg(DatabaseManager::instance().statusDescription());
        }
        return {};
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outError) *outError = QStringLiteral("Database is not currently open.");
        return {};
    }

    QString runwayType;
    if (runwayId > 0) {
        QSqlQuery rwQuery(db);
        rwQuery.prepare(QStringLiteral("SELECT runway_type FROM runways WHERE runway_id = :id;"));
        rwQuery.bindValue(QStringLiteral(":id"), runwayId);
        if (rwQuery.exec() && rwQuery.next()) {
            runwayType = rwQuery.value(0).toString();
        }
    }

    QString sql = QStringLiteral(
        "SELECT f.flight_id, f.flight_number, f.airline, f.source, f.destination, "
        "       f.flight_type, f.priority, f.is_emergency, f.operational_status, "
        "       f.scheduled_time, f.created_at, f.aircraft_id, "
        "       COALESCE(CONCAT(a.registration_number, ' (', a.aircraft_type, ')'), '') AS aircraft_reg "
        "FROM flights f "
        "LEFT JOIN aircraft a ON f.aircraft_id = a.aircraft_id "
        "WHERE f.operational_status NOT IN ('Landed', 'Departed', 'Cancelled') "
        "  AND f.flight_id NOT IN (SELECT flight_id FROM runway_assignments WHERE status = 'Active') "
        "  AND f.flight_id NOT IN (SELECT flight_id FROM scheduling_queue WHERE process_state = 'Completed') "
    );

    if (runwayType.compare(QStringLiteral("Landing"), Qt::CaseInsensitive) == 0) {
        sql += QStringLiteral("  AND f.flight_type = 'Arrival' ");
    } else if (runwayType.compare(QStringLiteral("Takeoff"), Qt::CaseInsensitive) == 0) {
        sql += QStringLiteral("  AND f.flight_type = 'Departure' ");
    }

    sql += QStringLiteral("ORDER BY f.is_emergency DESC, f.priority ASC, f.scheduled_time ASC;");

    QSqlQuery query(db);
    query.prepare(sql);
    if (!query.exec()) {
        qWarning() << "[ApplicationService] getEligibleFlightsForRunwayAllocation query failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to fetch eligible flights: %1").arg(query.lastError().text());
        }
        return {};
    }

    QList<Models::Flight> result;
    while (query.next()) {
        Models::Flight flight;
        flight.id                   = query.value(0).toInt();
        flight.flightNumber         = query.value(1).toString();
        flight.airline              = query.value(2).toString();
        flight.source               = query.value(3).toString();
        flight.origin               = flight.source;
        flight.destination         = query.value(4).toString();
        flight.flightType           = query.value(5).toString();
        flight.priority             = query.value(6).toInt();
        flight.isEmergency          = query.value(7).toBool();
        flight.operationalStatus    = query.value(8).toString();
        flight.scheduledTime        = query.value(9).toDateTime();
        flight.createdAt            = query.value(10).toDateTime();
        flight.aircraftId           = query.value(11).toInt();
        flight.aircraftRegistration = query.value(12).toString();

        result.append(flight);
    }

    return result;
}

QList<Models::Runway> ApplicationService::getAvailableRunwaysForAllocation(QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) {
            *outError = QStringLiteral("Authentication required: unauthenticated users cannot view available runways.");
        }
        return {};
    }

    if (!checkDatabaseConnection()) {
        if (outError) {
            *outError = QStringLiteral("Database connection is offline: %1")
                            .arg(DatabaseManager::instance().statusDescription());
        }
        return {};
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outError) *outError = QStringLiteral("Database is not currently open.");
        return {};
    }

    QString sql = QStringLiteral(
        "SELECT r.runway_id, r.runway_number, r.runway_type, r.status "
        "FROM runways r "
        "WHERE r.status = 'Available' "
        "  AND r.runway_id NOT IN (SELECT runway_id FROM runway_assignments WHERE status = 'Active') "
        "ORDER BY r.runway_number ASC;"
    );

    QSqlQuery query(db);
    query.prepare(sql);
    if (!query.exec()) {
        qWarning() << "[ApplicationService] getAvailableRunwaysForAllocation query failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to fetch available runways: %1").arg(query.lastError().text());
        }
        return {};
    }

    QList<Models::Runway> result;
    while (query.next()) {
        Models::Runway rw;
        rw.id           = query.value(0).toInt();
        rw.runwayNumber = query.value(1).toString();
        rw.name         = rw.runwayNumber;
        rw.runwayType   = query.value(2).toString();
        rw.status       = query.value(3).toString();
        result.append(rw);
    }

    return result;
}

bool ApplicationService::getActiveAssignmentForRunway(int runwayId, Models::RunwayAssignment &outAssignment, QString *outError)
{
    if (runwayId <= 0) {
        if (outError) *outError = QStringLiteral("Invalid runway ID.");
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
        "SELECT ra.assignment_id, ra.flight_id, ra.runway_id, ra.assigned_by, "
        "       ra.assigned_at, ra.released_at, ra.status, "
        "       f.flight_number, f.airline, r.runway_number, COALESCE(u.username, '') "
        "FROM runway_assignments ra "
        "JOIN flights f ON ra.flight_id = f.flight_id "
        "JOIN runways r ON ra.runway_id = r.runway_id "
        "LEFT JOIN users u ON ra.assigned_by = u.user_id "
        "WHERE ra.runway_id = :rid AND ra.status = 'Active';"
    ));
    query.bindValue(QStringLiteral(":rid"), runwayId);

    if (!query.exec() || !query.next()) {
        return false;
    }

    outAssignment.assignmentId       = query.value(0).toInt();
    outAssignment.flightId           = query.value(1).toInt();
    outAssignment.runwayId           = query.value(2).toInt();
    outAssignment.assignedBy         = query.value(3).toInt();
    outAssignment.assignedAt         = query.value(4).toDateTime();
    outAssignment.releasedAt         = query.value(5).toDateTime();
    outAssignment.status             = query.value(6).toString();
    outAssignment.flightNumber       = query.value(7).toString();
    outAssignment.airline            = query.value(8).toString();
    outAssignment.runwayNumber       = query.value(9).toString();
    outAssignment.assignedByUsername = query.value(10).toString();

    return true;
}

bool ApplicationService::getActiveAssignmentForFlight(int flightId, Models::RunwayAssignment &outAssignment, QString *outError)
{
    if (flightId <= 0) {
        if (outError) *outError = QStringLiteral("Invalid flight ID.");
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
        "SELECT ra.assignment_id, ra.flight_id, ra.runway_id, ra.assigned_by, "
        "       ra.assigned_at, ra.released_at, ra.status, "
        "       f.flight_number, f.airline, r.runway_number, COALESCE(u.username, '') "
        "FROM runway_assignments ra "
        "JOIN flights f ON ra.flight_id = f.flight_id "
        "JOIN runways r ON ra.runway_id = r.runway_id "
        "LEFT JOIN users u ON ra.assigned_by = u.user_id "
        "WHERE ra.flight_id = :fid AND ra.status = 'Active';"
    ));
    query.bindValue(QStringLiteral(":fid"), flightId);

    if (!query.exec() || !query.next()) {
        return false;
    }

    outAssignment.assignmentId       = query.value(0).toInt();
    outAssignment.flightId           = query.value(1).toInt();
    outAssignment.runwayId           = query.value(2).toInt();
    outAssignment.assignedBy         = query.value(3).toInt();
    outAssignment.assignedAt         = query.value(4).toDateTime();
    outAssignment.releasedAt         = query.value(5).toDateTime();
    outAssignment.status             = query.value(6).toString();
    outAssignment.flightNumber       = query.value(7).toString();
    outAssignment.airline            = query.value(8).toString();
    outAssignment.runwayNumber       = query.value(9).toString();
    outAssignment.assignedByUsername = query.value(10).toString();

    return true;
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

    if (runwayId <= 0) {
        if (outError) {
            *outError = QStringLiteral("Invalid runway ID: %1. Runway ID must be greater than zero.").arg(runwayId);
        }
        return false;
    }

    QString targetStatus = inMaintenance ? QStringLiteral("Maintenance") : QStringLiteral("Available");

    // If database connection is active, persist to MySQL
    DatabaseManager &dbManager = DatabaseManager::instance();
    if (dbManager.isConnected()) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery query(db);
            query.prepare(QStringLiteral("UPDATE runways SET status = :status WHERE runway_id = :id;"));
            query.bindValue(QStringLiteral(":status"), targetStatus);
            query.bindValue(QStringLiteral(":id"), runwayId);

            if (!query.exec()) {
                qWarning() << "[ApplicationService] setRunwayMaintenanceStatus failed:" << query.lastError().text();
                if (outError) {
                    *outError = QStringLiteral("Failed to update runway maintenance status: %1")
                                    .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
                }
                return false;
            }
        }
    }

    emit runwaysChanged();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Runway Management Methods (Day 14)
// ─────────────────────────────────────────────────────────────────────────────

QList<Models::Runway> ApplicationService::getAllRunways(const QString &searchFilter,
                                                        const QString &statusFilter,
                                                        QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) {
            *outError = QStringLiteral("Authentication required: unauthenticated users cannot view runways.");
        }
        return {};
    }

    if (!m_session.hasPermission(QStringLiteral("page:Runways"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to view runways.")
                            .arg(m_session.role());
        }
        return {};
    }

    if (!checkDatabaseConnection()) {
        if (outError) {
            *outError = QStringLiteral("Database connection is offline: %1")
                            .arg(DatabaseManager::instance().statusDescription());
        }
        return {};
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outError) *outError = QStringLiteral("Database is not currently open.");
        return {};
    }

    QString sql = QStringLiteral(
        "SELECT r.runway_id, r.runway_number, r.runway_type, r.status, "
        "       COALESCE(ra.flight_id, 0) AS active_flight_id, "
        "       COALESCE(f.flight_number, '') AS active_flight_number "
        "FROM runways r "
        "LEFT JOIN runway_assignments ra ON r.runway_id = ra.runway_id AND ra.status = 'Active' "
        "LEFT JOIN flights f ON ra.flight_id = f.flight_id "
        "WHERE 1=1 "
    );

    QString trimmedSearch = searchFilter.trimmed();
    if (!trimmedSearch.isEmpty()) {
        sql += QStringLiteral("AND (r.runway_number LIKE :search OR r.runway_type LIKE :search) ");
    }

    QString trimmedStatus = statusFilter.trimmed();
    if (!trimmedStatus.isEmpty() &&
        trimmedStatus.compare(QStringLiteral("All Statuses"), Qt::CaseInsensitive) != 0 &&
        trimmedStatus.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0) {
        sql += QStringLiteral("AND r.status = :status ");
    }

    sql += QStringLiteral("ORDER BY r.runway_id ASC;");

    QSqlQuery query(db);
    query.prepare(sql);

    if (!trimmedSearch.isEmpty()) {
        query.bindValue(QStringLiteral(":search"), QStringLiteral("%%1%").arg(trimmedSearch));
    }
    if (!trimmedStatus.isEmpty() &&
        trimmedStatus.compare(QStringLiteral("All Statuses"), Qt::CaseInsensitive) != 0 &&
        trimmedStatus.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0) {
        query.bindValue(QStringLiteral(":status"), trimmedStatus);
    }

    if (!query.exec()) {
        qWarning() << "[ApplicationService] getAllRunways query failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to retrieve runways: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return {};
    }

    QList<Models::Runway> results;
    while (query.next()) {
        Models::Runway rw;
        rw.id = query.value(0).toInt();
        rw.runwayNumber = query.value(1).toString();
        rw.name = rw.runwayNumber;
        rw.runwayType = query.value(2).toString();
        rw.status = query.value(3).toString();
        rw.activeFlightId = query.value(4).toInt();
        rw.activeFlightNumber = query.value(5).toString();
        results.append(rw);
    }

    return results;
}

bool ApplicationService::getRunwayById(int runwayId, Models::Runway &outRunway, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot view runways.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("page:Runways"))) {
        if (outError) *outError = QStringLiteral("Access denied: role '%1' is not authorized to view runways.").arg(m_session.role());
        return false;
    }

    if (runwayId <= 0) {
        if (outError) *outError = QStringLiteral("Invalid runway ID: %1. Must be greater than zero.").arg(runwayId);
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
        "SELECT r.runway_id, r.runway_number, r.runway_type, r.status, "
        "       COALESCE(ra.flight_id, 0) AS active_flight_id, "
        "       COALESCE(f.flight_number, '') AS active_flight_number "
        "FROM runways r "
        "LEFT JOIN runway_assignments ra ON r.runway_id = ra.runway_id AND ra.status = 'Active' "
        "LEFT JOIN flights f ON ra.flight_id = f.flight_id "
        "WHERE r.runway_id = :id "
        "LIMIT 1;"
    ));
    query.bindValue(QStringLiteral(":id"), runwayId);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] getRunwayById failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to fetch runway: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return false;
    }

    if (!query.next()) {
        if (outError) *outError = QStringLiteral("Runway with ID %1 was not found.").arg(runwayId);
        return false;
    }

    outRunway.id = query.value(0).toInt();
    outRunway.runwayNumber = query.value(1).toString();
    outRunway.name = outRunway.runwayNumber;
    outRunway.runwayType = query.value(2).toString();
    outRunway.status = query.value(3).toString();
    outRunway.activeFlightId = query.value(4).toInt();
    outRunway.activeFlightNumber = query.value(5).toString();

    return true;
}

bool ApplicationService::getRunwayByNumber(const QString &runwayNumber, Models::Runway &outRunway, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot view runways.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("page:Runways"))) {
        if (outError) *outError = QStringLiteral("Access denied: role '%1' is not authorized to view runways.").arg(m_session.role());
        return false;
    }

    QString trimmedNum = runwayNumber.trimmed().toUpper();
    if (trimmedNum.isEmpty()) {
        if (outError) *outError = QStringLiteral("Runway number is required.");
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
        "SELECT r.runway_id, r.runway_number, r.runway_type, r.status, "
        "       COALESCE(ra.flight_id, 0) AS active_flight_id, "
        "       COALESCE(f.flight_number, '') AS active_flight_number "
        "FROM runways r "
        "LEFT JOIN runway_assignments ra ON r.runway_id = ra.runway_id AND ra.status = 'Active' "
        "LEFT JOIN flights f ON ra.flight_id = f.flight_id "
        "WHERE r.runway_number = :num "
        "LIMIT 1;"
    ));
    query.bindValue(QStringLiteral(":num"), trimmedNum);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] getRunwayByNumber failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to fetch runway: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return false;
    }

    if (!query.next()) {
        if (outError) *outError = QStringLiteral("Runway with designation '%1' was not found.").arg(trimmedNum);
        return false;
    }

    outRunway.id = query.value(0).toInt();
    outRunway.runwayNumber = query.value(1).toString();
    outRunway.name = outRunway.runwayNumber;
    outRunway.runwayType = query.value(2).toString();
    outRunway.status = query.value(3).toString();
    outRunway.activeFlightId = query.value(4).toInt();
    outRunway.activeFlightNumber = query.value(5).toString();

    return true;
}

bool ApplicationService::updateRunwayStatus(int runwayId, const QString &newStatus, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) {
            *outError = QStringLiteral("Authentication required: unauthenticated users cannot update runway status.");
        }
        return false;
    }

    bool isOperator = m_session.hasPermission(QStringLiteral("action:RunwayMaintenance"));
    bool isAdmin = m_session.isAdmin();

    if (!isOperator && !isAdmin) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to update runway status.")
                            .arg(m_session.role());
        }
        return false;
    }

    if (runwayId <= 0) {
        if (outError) {
            *outError = QStringLiteral("Invalid runway ID: %1. Must be greater than zero.").arg(runwayId);
        }
        return false;
    }

    static const QStringList validStatuses = {
        QStringLiteral("Available"),
        QStringLiteral("Occupied"),
        QStringLiteral("Maintenance")
    };

    QString trimmedStatus = newStatus.trimmed();
    if (!validStatuses.contains(trimmedStatus, Qt::CaseInsensitive)) {
        if (outError) {
            *outError = QStringLiteral("Invalid runway status: '%1'. Must be Available, Occupied, or Maintenance.").arg(newStatus);
        }
        return false;
    }

    // Match case with database ENUM
    for (const QString &canonical : validStatuses) {
        if (trimmedStatus.compare(canonical, Qt::CaseInsensitive) == 0) {
            trimmedStatus = canonical;
            break;
        }
    }

    // Airport Operator may only manage maintenance status
    if (isOperator && !isAdmin) {
        if (trimmedStatus == QStringLiteral("Occupied")) {
            if (outError) {
                *outError = QStringLiteral("Access denied: Airport Operators cannot manually set runway to Occupied. Occupancy is managed through flight allocation.");
            }
            return false;
        }
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
    query.prepare(QStringLiteral("UPDATE runways SET status = :status WHERE runway_id = :id;"));
    query.bindValue(QStringLiteral(":status"), trimmedStatus);
    query.bindValue(QStringLiteral(":id"), runwayId);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] updateRunwayStatus failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to update runway status: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return false;
    }

    if (query.numRowsAffected() == 0) {
        Models::Runway checkRw;
        if (!getRunwayById(runwayId, checkRw, outError)) {
            if (outError && outError->isEmpty()) {
                *outError = QStringLiteral("Runway with ID %1 was not found.").arg(runwayId);
            }
            return false;
        }
    }

    emit runwaysChanged();
    return true;
}

bool ApplicationService::createRunway(const Models::Runway &runway, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot create runways.");
        return false;
    }

    if (!m_session.isAdmin()) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to create runways (Admin-only).")
                            .arg(m_session.role());
        }
        return false;
    }

    Models::Runway rw = runway;
    rw.normalize();
    if (!rw.validate(outError)) {
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

    // Check duplicate runway_number
    QSqlQuery checkQuery(db);
    checkQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM runways WHERE runway_number = :num;"));
    checkQuery.bindValue(QStringLiteral(":num"), rw.runwayNumber);
    if (checkQuery.exec() && checkQuery.next()) {
        if (checkQuery.value(0).toInt() > 0) {
            if (outError) {
                *outError = QStringLiteral("A runway with designation '%1' already exists.").arg(rw.runwayNumber);
            }
            return false;
        }
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO runways (runway_number, runway_type, status) "
        "VALUES (:num, :type, :status);"
    ));
    query.bindValue(QStringLiteral(":num"), rw.runwayNumber);
    query.bindValue(QStringLiteral(":type"), rw.runwayType);
    query.bindValue(QStringLiteral(":status"), rw.status);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] createRunway failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to create runway: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return false;
    }

    emit runwaysChanged();
    return true;
}

bool ApplicationService::updateRunway(const Models::Runway &runway, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot update runways.");
        return false;
    }

    if (runway.id <= 0) {
        if (outError) *outError = QStringLiteral("Invalid runway ID: %1. Must be greater than zero.").arg(runway.id);
        return false;
    }

    Models::Runway existing;
    if (!getRunwayById(runway.id, existing, outError)) {
        return false;
    }

    bool isOperator = m_session.hasPermission(QStringLiteral("action:RunwayMaintenance"));
    bool isAdmin = m_session.isAdmin();

    if (!isOperator && !isAdmin) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to edit runways.")
                            .arg(m_session.role());
        }
        return false;
    }

    if (isOperator && !isAdmin) {
        if (existing.effectiveNumber() != runway.effectiveNumber() ||
            existing.runwayType != runway.runwayType) {
            if (outError) {
                *outError = QStringLiteral("Access denied: Airport Operators may only update runway maintenance status. "
                                           "Modifying runway designation or type requires Administrator privileges.");
            }
            return false;
        }
        return updateRunwayStatus(runway.id, runway.status, outError);
    }

    Models::Runway rw = runway;
    rw.normalize();
    if (!rw.validate(outError)) {
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

    // Check duplicate runway_number
    QSqlQuery checkQuery(db);
    checkQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM runways WHERE runway_number = :num AND runway_id != :id;"));
    checkQuery.bindValue(QStringLiteral(":num"), rw.runwayNumber);
    checkQuery.bindValue(QStringLiteral(":id"), rw.id);
    if (checkQuery.exec() && checkQuery.next()) {
        if (checkQuery.value(0).toInt() > 0) {
            if (outError) {
                *outError = QStringLiteral("A runway with designation '%1' already exists.").arg(rw.runwayNumber);
            }
            return false;
        }
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE runways SET "
        "  runway_number = :num, "
        "  runway_type = :type, "
        "  status = :status "
        "WHERE runway_id = :id;"
    ));
    query.bindValue(QStringLiteral(":num"), rw.runwayNumber);
    query.bindValue(QStringLiteral(":type"), rw.runwayType);
    query.bindValue(QStringLiteral(":status"), rw.status);
    query.bindValue(QStringLiteral(":id"), rw.id);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] updateRunway failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to update runway: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return false;
    }

    emit runwaysChanged();
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

    if (m_scheduler) {
        m_scheduler->readyQueue().removeByFlightId(flightId);
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

// ─────────────────────────────────────────────────────────────────────────────
// Aircraft Management Methods (Day 8)
// ─────────────────────────────────────────────────────────────────────────────

QList<Models::Aircraft> ApplicationService::getAllAircraft(const QString &searchFilter,
                                                          const QString &statusFilter,
                                                          QString *outError)
{
    QList<Models::Aircraft> results;

    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot view aircraft.");
        return results;
    }

    if (!m_session.hasPermission(QStringLiteral("page:Aircraft"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to view aircraft.")
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
        "SELECT a.aircraft_id, a.registration_number, a.aircraft_type, a.capacity, a.status, "
        "       (SELECT COUNT(*) FROM flights f WHERE f.aircraft_id = a.aircraft_id) AS flight_count "
        "FROM aircraft a "
        "WHERE 1=1"
    );

    QString trimmedSearch = searchFilter.trimmed();
    bool hasSearch = !trimmedSearch.isEmpty();
    if (hasSearch) {
        sql += QStringLiteral(" AND (a.registration_number LIKE :search OR a.aircraft_type LIKE :search)");
    }

    QString trimmedStatus = statusFilter.trimmed();
    bool hasStatus = !trimmedStatus.isEmpty() && trimmedStatus.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0;
    if (hasStatus) {
        sql += QStringLiteral(" AND a.status = :status");
    }

    sql += QStringLiteral(" ORDER BY (a.status = 'Active') DESC, (a.status = 'Maintenance') DESC, a.registration_number ASC;");

    QSqlQuery query(db);
    query.prepare(sql);

    if (hasSearch) {
        query.bindValue(QStringLiteral(":search"), QStringLiteral("%%1%").arg(trimmedSearch));
    }
    if (hasStatus) {
        query.bindValue(QStringLiteral(":status"), trimmedStatus);
    }

    if (!query.exec()) {
        qWarning() << "[ApplicationService] getAllAircraft query failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to fetch aircraft: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return results;
    }

    while (query.next()) {
        Models::Aircraft ac;
        ac.id = query.value(QStringLiteral("aircraft_id")).toInt();
        ac.registrationNumber = query.value(QStringLiteral("registration_number")).toString();
        ac.registrationNo = ac.registrationNumber;
        ac.aircraftType = query.value(QStringLiteral("aircraft_type")).toString();
        ac.model = ac.aircraftType;
        ac.capacity = query.value(QStringLiteral("capacity")).toInt();
        ac.status = query.value(QStringLiteral("status")).toString();
        ac.assignedFlightsCount = query.value(QStringLiteral("flight_count")).toInt();
        results.append(ac);
    }

    return results;
}

bool ApplicationService::getAircraftById(int aircraftId, Models::Aircraft &outAircraft, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot view aircraft.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("page:Aircraft"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to view aircraft.")
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
        "SELECT a.aircraft_id, a.registration_number, a.aircraft_type, a.capacity, a.status, "
        "       (SELECT COUNT(*) FROM flights f WHERE f.aircraft_id = a.aircraft_id) AS flight_count "
        "FROM aircraft a "
        "WHERE a.aircraft_id = :id "
        "LIMIT 1;"
    ));
    query.bindValue(QStringLiteral(":id"), aircraftId);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] getAircraftById query failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to fetch aircraft: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return false;
    }

    if (!query.next()) {
        if (outError) *outError = QStringLiteral("Aircraft record not found.");
        return false;
    }

    outAircraft.id = query.value(QStringLiteral("aircraft_id")).toInt();
    outAircraft.registrationNumber = query.value(QStringLiteral("registration_number")).toString();
    outAircraft.registrationNo = outAircraft.registrationNumber;
    outAircraft.aircraftType = query.value(QStringLiteral("aircraft_type")).toString();
    outAircraft.model = outAircraft.aircraftType;
    outAircraft.capacity = query.value(QStringLiteral("capacity")).toInt();
    outAircraft.status = query.value(QStringLiteral("status")).toString();
    outAircraft.assignedFlightsCount = query.value(QStringLiteral("flight_count")).toInt();
    return true;
}

bool ApplicationService::getAircraftByRegistration(const QString &registration, Models::Aircraft &outAircraft, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot view aircraft.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("page:Aircraft"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to view aircraft.")
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
        "SELECT a.aircraft_id, a.registration_number, a.aircraft_type, a.capacity, a.status, "
        "       (SELECT COUNT(*) FROM flights f WHERE f.aircraft_id = a.aircraft_id) AS flight_count "
        "FROM aircraft a "
        "WHERE a.registration_number = :reg "
        "LIMIT 1;"
    ));
    query.bindValue(QStringLiteral(":reg"), registration.trimmed().toUpper());

    if (!query.exec()) {
        qWarning() << "[ApplicationService] getAircraftByRegistration query failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to fetch aircraft: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return false;
    }

    if (!query.next()) {
        if (outError) *outError = QStringLiteral("Aircraft not found with registration '%1'.").arg(registration);
        return false;
    }

    outAircraft.id = query.value(QStringLiteral("aircraft_id")).toInt();
    outAircraft.registrationNumber = query.value(QStringLiteral("registration_number")).toString();
    outAircraft.registrationNo = outAircraft.registrationNumber;
    outAircraft.aircraftType = query.value(QStringLiteral("aircraft_type")).toString();
    outAircraft.model = outAircraft.aircraftType;
    outAircraft.capacity = query.value(QStringLiteral("capacity")).toInt();
    outAircraft.status = query.value(QStringLiteral("status")).toString();
    outAircraft.assignedFlightsCount = query.value(QStringLiteral("flight_count")).toInt();
    return true;
}

bool ApplicationService::createAircraft(Models::Aircraft &aircraft, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot register aircraft.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:AdministerAircraft"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: Only Administrators can register new aircraft (role '%1' is not authorized).")
                            .arg(m_session.role());
        }
        return false;
    }

    if (!aircraft.validate(outError)) {
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

    QString normalizedReg = aircraft.effectiveRegistration();

    // Check unique registration constraint
    QSqlQuery checkQuery(db);
    checkQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM aircraft WHERE registration_number = :reg;"));
    checkQuery.bindValue(QStringLiteral(":reg"), normalizedReg);
    if (checkQuery.exec() && checkQuery.next()) {
        if (checkQuery.value(0).toInt() > 0) {
            if (outError) {
                *outError = QStringLiteral("An aircraft with registration '%1' is already registered.").arg(normalizedReg);
            }
            return false;
        }
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO aircraft (registration_number, aircraft_type, capacity, status) "
        "VALUES (:reg, :type, :capacity, :status);"
    ));
    query.bindValue(QStringLiteral(":reg"), normalizedReg);
    query.bindValue(QStringLiteral(":type"), aircraft.effectiveType());
    query.bindValue(QStringLiteral(":capacity"), aircraft.capacity);
    query.bindValue(QStringLiteral(":status"), aircraft.status.trimmed());

    if (!query.exec()) {
        qWarning() << "[ApplicationService] createAircraft failed:" << query.lastError().text();
        if (outError) {
            QString dbErr = query.lastError().databaseText();
            if (dbErr.contains(QStringLiteral("Duplicate"), Qt::CaseInsensitive) ||
                query.lastError().nativeErrorCode() == QStringLiteral("1062")) {
                *outError = QStringLiteral("An aircraft with registration '%1' is already registered.").arg(normalizedReg);
            } else {
                *outError = QStringLiteral("Failed to insert aircraft: %1")
                                .arg(dbErr.isEmpty() ? query.lastError().text() : dbErr);
            }
        }
        return false;
    }

    aircraft.id = query.lastInsertId().toInt();

    emit aircraftChanged();
    emit flightsChanged();
    return true;
}

bool ApplicationService::createAircraft(const Models::Aircraft &aircraft, QString *outError)
{
    Models::Aircraft copy = aircraft;
    return createAircraft(copy, outError);
}

bool ApplicationService::updateAircraft(const Models::Aircraft &aircraft, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot update aircraft.");
        return false;
    }

    if (aircraft.id <= 0) {
        if (outError) *outError = QStringLiteral("Invalid aircraft ID for update.");
        return false;
    }

    if (!aircraft.validate(outError)) {
        return false;
    }

    Models::Aircraft existing;
    if (!getAircraftById(aircraft.id, existing, outError)) {
        return false;
    }

    // Role-specific update enforcement:
    // Airport Operator: May ONLY update operational status.
    // Admin: May update all fields.
    if (m_session.isOperator()) {
        if (existing.effectiveRegistration() != aircraft.effectiveRegistration() ||
            existing.effectiveType() != aircraft.effectiveType() ||
            existing.capacity != aircraft.capacity) {
            if (outError) {
                *outError = QStringLiteral("Access denied: Airport Operators may only update aircraft operational status. "
                                           "Modifying registration, type, or capacity requires Administrator privileges.");
            }
            return false;
        }
        return updateAircraftStatus(aircraft.id, aircraft.status, outError);
    }

    if (!m_session.hasPermission(QStringLiteral("action:AdministerAircraft"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to edit aircraft.")
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

    QString normalizedReg = aircraft.effectiveRegistration();

    // Check unique constraint against other aircraft
    QSqlQuery checkQuery(db);
    checkQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM aircraft WHERE registration_number = :reg AND aircraft_id != :id;"));
    checkQuery.bindValue(QStringLiteral(":reg"), normalizedReg);
    checkQuery.bindValue(QStringLiteral(":id"), aircraft.id);
    if (checkQuery.exec() && checkQuery.next()) {
        if (checkQuery.value(0).toInt() > 0) {
            if (outError) {
                *outError = QStringLiteral("An aircraft with registration '%1' already exists.").arg(normalizedReg);
            }
            return false;
        }
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE aircraft SET "
        "  registration_number = :reg, "
        "  aircraft_type = :type, "
        "  capacity = :capacity, "
        "  status = :status "
        "WHERE aircraft_id = :id;"
    ));
    query.bindValue(QStringLiteral(":reg"), normalizedReg);
    query.bindValue(QStringLiteral(":type"), aircraft.effectiveType());
    query.bindValue(QStringLiteral(":capacity"), aircraft.capacity);
    query.bindValue(QStringLiteral(":status"), aircraft.status.trimmed());
    query.bindValue(QStringLiteral(":id"), aircraft.id);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] updateAircraft failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to update aircraft: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return false;
    }

    emit aircraftChanged();
    emit flightsChanged();
    return true;
}

bool ApplicationService::updateAircraftStatus(int aircraftId, const QString &newStatus, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot update aircraft status.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:ManageAircraft"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to update aircraft status.")
                            .arg(m_session.role());
        }
        return false;
    }

    if (aircraftId <= 0) {
        if (outError) *outError = QStringLiteral("Invalid aircraft ID.");
        return false;
    }

    static const QStringList validStatuses = {
        QStringLiteral("Active"),
        QStringLiteral("Maintenance"),
        QStringLiteral("Retired")
    };
    if (!validStatuses.contains(newStatus.trimmed())) {
        if (outError) {
            *outError = QStringLiteral("Invalid status '%1'. Permitted values: Active, Maintenance, Retired.").arg(newStatus);
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
    query.prepare(QStringLiteral("UPDATE aircraft SET status = :status WHERE aircraft_id = :id;"));
    query.bindValue(QStringLiteral(":status"), newStatus.trimmed());
    query.bindValue(QStringLiteral(":id"), aircraftId);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] updateAircraftStatus failed:" << query.lastError().text();
        if (outError) {
            *outError = QStringLiteral("Failed to update aircraft status: %1")
                            .arg(query.lastError().databaseText().isEmpty() ? query.lastError().text() : query.lastError().databaseText());
        }
        return false;
    }

    if (query.numRowsAffected() == 0) {
        // Confirm whether record exists
        Models::Aircraft check;
        if (!getAircraftById(aircraftId, check, outError)) {
            return false;
        }
    }

    emit aircraftChanged();
    emit flightsChanged();
    return true;
}

bool ApplicationService::canDeleteAircraft(int aircraftId, QString *outReason)
{
    if (aircraftId <= 0) {
        if (outReason) *outReason = QStringLiteral("Invalid aircraft ID.");
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

    // Check if flights reference this aircraft
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM flights WHERE aircraft_id = :id;"));
    q.bindValue(QStringLiteral(":id"), aircraftId);
    if (q.exec() && q.next()) {
        int count = q.value(0).toInt();
        if (count > 0) {
            if (outReason) {
                *outReason = QStringLiteral(
                    "Cannot delete aircraft: It is referenced by %1 flight record(s). "
                    "To preserve flight operation history and operational integrity, "
                    "retirement ('Retired') should be used instead of deleting the aircraft record."
                ).arg(count);
            }
            return false;
        }
    }

    return true;
}

bool ApplicationService::deleteAircraft(int aircraftId, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot delete aircraft.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:AdministerAircraft"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: Only Administrators can delete aircraft (role '%1' is not authorized).")
                            .arg(m_session.role());
        }
        return false;
    }

    QString blockReason;
    if (!canDeleteAircraft(aircraftId, &blockReason)) {
        if (outError) *outError = blockReason;
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outError) *outError = QStringLiteral("Database is not currently open.");
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM aircraft WHERE aircraft_id = :id;"));
    query.bindValue(QStringLiteral(":id"), aircraftId);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] deleteAircraft query failed:" << query.lastError().text();
        QString dbErr = query.lastError().databaseText();
        if (outError) {
            *outError = QStringLiteral("Database delete failed: %1")
                            .arg(dbErr.isEmpty() ? query.lastError().text() : dbErr);
        }
        return false;
    }

    if (query.numRowsAffected() == 0) {
        if (outError) *outError = QStringLiteral("Aircraft not found or already deleted.");
        return false;
    }

    emit aircraftChanged();
    emit flightsChanged();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Day 9: Process Model (PCB) and Scheduling Queue Integration
// ─────────────────────────────────────────────────────────────────────────────

bool ApplicationService::createProcessForFlight(int flightId,
                                                int serviceTimeSeconds,
                                                Models::Process *outProcess,
                                                QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot enqueue flights.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:ManageScheduling"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to create scheduling processes (ATC/Admin only).")
                            .arg(m_session.role());
        }
        return false;
    }

    if (serviceTimeSeconds <= 0 || serviceTimeSeconds > 65535) {
        if (outError) *outError = QStringLiteral("Service time must be between 1 and 65535 seconds.");
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

    // 1. Fetch flight to verify existence and retrieve attributes
    Models::Flight flight;
    if (!getFlightById(flightId, flight, outError)) {
        return false;
    }

    // 2. Check flight eligibility (cannot be Landed, Departed, or Cancelled)
    QString ineligibilityReason;
    if (!Models::Process::isFlightEligible(flight, &ineligibilityReason)) {
        if (outError) *outError = ineligibilityReason;
        return false;
    }

    // 3. Prevent duplicate active process record for this flight (enforce 1:0..1)
    QSqlQuery checkQuery(db);
    checkQuery.prepare(QStringLiteral("SELECT queue_id FROM scheduling_queue WHERE flight_id = :flightId LIMIT 1;"));
    checkQuery.bindValue(QStringLiteral(":flightId"), flightId);
    if (!checkQuery.exec()) {
        qWarning() << "[ApplicationService] Duplicate check query failed:" << checkQuery.lastError().text();
        if (outError) *outError = QStringLiteral("Failed to check existing scheduling record: %1").arg(checkQuery.lastError().text());
        return false;
    }
    if (checkQuery.next()) {
        if (outError) {
            *outError = QStringLiteral("Flight %1 (ID: %2) already has an active scheduling record.")
                            .arg(flight.flightNumber.isEmpty() ? QString::number(flightId) : flight.flightNumber)
                            .arg(flightId);
        }
        return false;
    }

    // 4. Map flight to simulated Process (PCB)
    Models::Process proc = Models::Process::fromFlight(flight, serviceTimeSeconds);
    QString validationError;
    if (!proc.validate(&validationError)) {
        if (outError) *outError = validationError;
        return false;
    }

    // 5. Parameterized insert into scheduling_queue table
    QSqlQuery insertQuery(db);
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO scheduling_queue ("
        "    flight_id, arrival_time, base_priority, effective_priority, service_time, process_state"
        ") VALUES ("
        "    :flightId, :arrivalTime, :basePriority, :effectivePriority, :serviceTime, :processState"
        ");"
    ));
    insertQuery.bindValue(QStringLiteral(":flightId"), proc.flightId);
    insertQuery.bindValue(QStringLiteral(":arrivalTime"), proc.arrivalTime);
    insertQuery.bindValue(QStringLiteral(":basePriority"), proc.basePriority);
    insertQuery.bindValue(QStringLiteral(":effectivePriority"), proc.effectivePriority);
    insertQuery.bindValue(QStringLiteral(":serviceTime"), proc.serviceTime);
    insertQuery.bindValue(QStringLiteral(":processState"), Models::Process::stateToDbString(proc.state));

    if (!insertQuery.exec()) {
        qWarning() << "[ApplicationService] createProcessForFlight INSERT failed:" << insertQuery.lastError().text();
        if (outError) {
            QString dbErr = insertQuery.lastError().databaseText();
            if (dbErr.contains(QStringLiteral("Duplicate entry"), Qt::CaseInsensitive) ||
                insertQuery.lastError().nativeErrorCode() == QStringLiteral("1062")) {
                *outError = QStringLiteral("Flight %1 (ID: %2) already has an active scheduling record.")
                                .arg(flight.flightNumber.isEmpty() ? QString::number(flightId) : flight.flightNumber)
                                .arg(flightId);
            } else {
                *outError = QStringLiteral("Failed to insert process into scheduling queue: %1")
                                .arg(dbErr.isEmpty() ? insertQuery.lastError().text() : dbErr);
            }
        }
        return false;
    }

    int assignedQueueId = insertQuery.lastInsertId().toInt();
    proc.setQueueId(assignedQueueId);

    if (m_scheduler && proc.state == Models::ProcessState::Ready) {
        m_scheduler->readyQueue().enqueue(proc);
    }

    if (outProcess) {
        *outProcess = proc;
    }

    emit processesChanged();
    return true;
}

bool ApplicationService::createProcess(const Models::Process &process,
                                       Models::Process *outProcess,
                                       QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot create processes.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:ManageScheduling"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to create scheduling processes (ATC/Admin only).")
                            .arg(m_session.role());
        }
        return false;
    }

    QString validationError;
    if (!process.validate(&validationError)) {
        if (outError) *outError = validationError;
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

    // Verify referenced flight exists and is eligible
    Models::Flight flight;
    if (!getFlightById(process.flightId, flight, outError)) {
        return false;
    }

    QString ineligibilityReason;
    if (!Models::Process::isFlightEligible(flight, &ineligibilityReason)) {
        if (outError) *outError = ineligibilityReason;
        return false;
    }

    // Check duplicate
    QSqlQuery checkQuery(db);
    checkQuery.prepare(QStringLiteral("SELECT queue_id FROM scheduling_queue WHERE flight_id = :flightId LIMIT 1;"));
    checkQuery.bindValue(QStringLiteral(":flightId"), process.flightId);
    if (!checkQuery.exec()) {
        if (outError) *outError = QStringLiteral("Failed to check existing scheduling record: %1").arg(checkQuery.lastError().text());
        return false;
    }
    if (checkQuery.next()) {
        if (outError) {
            *outError = QStringLiteral("Flight %1 (ID: %2) already has an active scheduling record.")
                            .arg(flight.flightNumber.isEmpty() ? QString::number(process.flightId) : flight.flightNumber)
                            .arg(process.flightId);
        }
        return false;
    }

    QSqlQuery insertQuery(db);
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO scheduling_queue ("
        "    flight_id, arrival_time, base_priority, effective_priority, service_time, process_state"
        ") VALUES ("
        "    :flightId, :arrivalTime, :basePriority, :effectivePriority, :serviceTime, :processState"
        ");"
    ));
    insertQuery.bindValue(QStringLiteral(":flightId"), process.flightId);
    insertQuery.bindValue(QStringLiteral(":arrivalTime"), process.arrivalTime);
    insertQuery.bindValue(QStringLiteral(":basePriority"), process.basePriority);
    insertQuery.bindValue(QStringLiteral(":effectivePriority"), process.effectivePriority);
    insertQuery.bindValue(QStringLiteral(":serviceTime"), process.serviceTime);
    insertQuery.bindValue(QStringLiteral(":processState"), Models::Process::stateToDbString(process.state));

    if (!insertQuery.exec()) {
        qWarning() << "[ApplicationService] createProcess INSERT failed:" << insertQuery.lastError().text();
        if (outError) {
            QString dbErr = insertQuery.lastError().databaseText();
            if (dbErr.contains(QStringLiteral("Duplicate entry"), Qt::CaseInsensitive) ||
                insertQuery.lastError().nativeErrorCode() == QStringLiteral("1062")) {
                *outError = QStringLiteral("Flight %1 (ID: %2) already has an active scheduling record.")
                                .arg(flight.flightNumber.isEmpty() ? QString::number(process.flightId) : flight.flightNumber)
                                .arg(process.flightId);
            } else {
                *outError = QStringLiteral("Failed to insert process into scheduling queue: %1")
                                .arg(dbErr.isEmpty() ? insertQuery.lastError().text() : dbErr);
            }
        }
        return false;
    }

    Models::Process created = process;
    created.setQueueId(insertQuery.lastInsertId().toInt());
    if (created.flightNumber.isEmpty()) created.flightNumber = flight.flightNumber;
    if (created.airline.isEmpty()) created.airline = flight.airline;
    if (created.operationalStatus.isEmpty()) created.operationalStatus = flight.operationalStatus;

    if (m_scheduler && created.state == Models::ProcessState::Ready) {
        m_scheduler->readyQueue().enqueue(created);
    }

    if (outProcess) {
        *outProcess = created;
    }

    emit processesChanged();
    return true;
}

bool ApplicationService::hasProcessForFlight(int flightId, QString *outError)
{
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
    query.prepare(QStringLiteral("SELECT queue_id FROM scheduling_queue WHERE flight_id = :id LIMIT 1;"));
    query.bindValue(QStringLiteral(":id"), flightId);

    if (!query.exec()) {
        if (outError) *outError = QStringLiteral("Failed to query scheduling queue: %1").arg(query.lastError().text());
        return false;
    }

    return query.next();
}

bool ApplicationService::getProcessByFlightId(int flightId, Models::Process &outProcess, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required to view scheduling queue.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("page:Scheduling"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to view scheduling processes.")
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
        "SELECT q.queue_id, q.flight_id, q.arrival_time, q.base_priority, q.effective_priority, "
        "       q.service_time, q.waiting_time, q.turnaround_time, q.completion_time, q.process_state, "
        "       f.flight_number, f.airline, f.operational_status "
        "FROM scheduling_queue q "
        "LEFT JOIN flights f ON q.flight_id = f.flight_id "
        "WHERE q.flight_id = :flightId "
        "LIMIT 1;"
    ));
    query.bindValue(QStringLiteral(":flightId"), flightId);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] getProcessByFlightId query failed:" << query.lastError().text();
        if (outError) *outError = QStringLiteral("Failed to fetch process: %1").arg(query.lastError().text());
        return false;
    }

    if (!query.next()) {
        if (outError) *outError = QStringLiteral("Process not found for flight ID %1.").arg(flightId);
        return false;
    }

    outProcess.setQueueId(query.value(QStringLiteral("queue_id")).toInt());
    outProcess.flightId = query.value(QStringLiteral("flight_id")).toInt();
    outProcess.arrivalTime = query.value(QStringLiteral("arrival_time")).toDateTime();
    outProcess.basePriority = query.value(QStringLiteral("base_priority")).toInt();
    outProcess.effectivePriority = query.value(QStringLiteral("effective_priority")).toInt();
    outProcess.serviceTime = query.value(QStringLiteral("service_time")).toInt();
    outProcess.waitingTime = query.value(QStringLiteral("waiting_time")).isNull() ? -1 : query.value(QStringLiteral("waiting_time")).toInt();
    outProcess.turnaroundTime = query.value(QStringLiteral("turnaround_time")).isNull() ? -1 : query.value(QStringLiteral("turnaround_time")).toInt();
    outProcess.completionTime = query.value(QStringLiteral("completion_time")).isNull() ? QDateTime() : query.value(QStringLiteral("completion_time")).toDateTime();
    outProcess.state = Models::Process::stateFromString(query.value(QStringLiteral("process_state")).toString());
    outProcess.flightNumber = query.value(QStringLiteral("flight_number")).toString();
    outProcess.airline = query.value(QStringLiteral("airline")).toString();
    outProcess.operationalStatus = query.value(QStringLiteral("operational_status")).toString();

    return true;
}

bool ApplicationService::getProcessById(int queueId, Models::Process &outProcess, QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required to view scheduling queue.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("page:Scheduling"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to view scheduling processes.")
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
        "SELECT q.queue_id, q.flight_id, q.arrival_time, q.base_priority, q.effective_priority, "
        "       q.service_time, q.waiting_time, q.turnaround_time, q.completion_time, q.process_state, "
        "       f.flight_number, f.airline, f.operational_status "
        "FROM scheduling_queue q "
        "LEFT JOIN flights f ON q.flight_id = f.flight_id "
        "WHERE q.queue_id = :queueId "
        "LIMIT 1;"
    ));
    query.bindValue(QStringLiteral(":queueId"), queueId);

    if (!query.exec()) {
        qWarning() << "[ApplicationService] getProcessById query failed:" << query.lastError().text();
        if (outError) *outError = QStringLiteral("Failed to fetch process: %1").arg(query.lastError().text());
        return false;
    }

    if (!query.next()) {
        if (outError) *outError = QStringLiteral("Process not found for queue ID %1.").arg(queueId);
        return false;
    }

    outProcess.setQueueId(query.value(QStringLiteral("queue_id")).toInt());
    outProcess.flightId = query.value(QStringLiteral("flight_id")).toInt();
    outProcess.arrivalTime = query.value(QStringLiteral("arrival_time")).toDateTime();
    outProcess.basePriority = query.value(QStringLiteral("base_priority")).toInt();
    outProcess.effectivePriority = query.value(QStringLiteral("effective_priority")).toInt();
    outProcess.serviceTime = query.value(QStringLiteral("service_time")).toInt();
    outProcess.waitingTime = query.value(QStringLiteral("waiting_time")).isNull() ? -1 : query.value(QStringLiteral("waiting_time")).toInt();
    outProcess.turnaroundTime = query.value(QStringLiteral("turnaround_time")).isNull() ? -1 : query.value(QStringLiteral("turnaround_time")).toInt();
    outProcess.completionTime = query.value(QStringLiteral("completion_time")).isNull() ? QDateTime() : query.value(QStringLiteral("completion_time")).toDateTime();
    outProcess.state = Models::Process::stateFromString(query.value(QStringLiteral("process_state")).toString());
    outProcess.flightNumber = query.value(QStringLiteral("flight_number")).toString();
    outProcess.airline = query.value(QStringLiteral("airline")).toString();
    outProcess.operationalStatus = query.value(QStringLiteral("operational_status")).toString();

    return true;
}

QList<Models::Process> ApplicationService::getAllProcesses(QString *outError)
{
    QList<Models::Process> results;

    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required to view scheduling queue.");
        return results;
    }

    if (!m_session.hasPermission(QStringLiteral("page:Scheduling"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to view scheduling processes.")
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

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT q.queue_id, q.flight_id, q.arrival_time, q.base_priority, q.effective_priority, "
        "       q.service_time, q.waiting_time, q.turnaround_time, q.completion_time, q.process_state, "
        "       f.flight_number, f.airline, f.operational_status "
        "FROM scheduling_queue q "
        "LEFT JOIN flights f ON q.flight_id = f.flight_id "
        "ORDER BY q.queue_id ASC;"
    ));

    if (!query.exec()) {
        qWarning() << "[ApplicationService] getAllProcesses query failed:" << query.lastError().text();
        if (outError) *outError = QStringLiteral("Failed to fetch processes: %1").arg(query.lastError().text());
        return results;
    }

    while (query.next()) {
        Models::Process proc;
        proc.setQueueId(query.value(QStringLiteral("queue_id")).toInt());
        proc.flightId = query.value(QStringLiteral("flight_id")).toInt();
        proc.arrivalTime = query.value(QStringLiteral("arrival_time")).toDateTime();
        proc.basePriority = query.value(QStringLiteral("base_priority")).toInt();
        proc.effectivePriority = query.value(QStringLiteral("effective_priority")).toInt();
        proc.serviceTime = query.value(QStringLiteral("service_time")).toInt();
        proc.waitingTime = query.value(QStringLiteral("waiting_time")).isNull() ? -1 : query.value(QStringLiteral("waiting_time")).toInt();
        proc.turnaroundTime = query.value(QStringLiteral("turnaround_time")).isNull() ? -1 : query.value(QStringLiteral("turnaround_time")).toInt();
        proc.completionTime = query.value(QStringLiteral("completion_time")).isNull() ? QDateTime() : query.value(QStringLiteral("completion_time")).toDateTime();
        proc.state = Models::Process::stateFromString(query.value(QStringLiteral("process_state")).toString());
        proc.flightNumber = query.value(QStringLiteral("flight_number")).toString();
        proc.airline = query.value(QStringLiteral("airline")).toString();
        proc.operationalStatus = query.value(QStringLiteral("operational_status")).toString();

        results.append(proc);
    }

    return results;
}

// ─────────────────────────────────────────────────────────────────────────────
// Day 10: Process States and Ready Queue Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

SchedulerComponents::ReadyQueue* ApplicationService::readyQueue() const
{
    return m_scheduler ? &(m_scheduler->readyQueue()) : nullptr;
}

bool ApplicationService::enqueueProcess(const Models::Process &process, QString *outError)
{
    if (!m_scheduler) {
        if (outError) *outError = QStringLiteral("Scheduler service is not initialized.");
        return false;
    }
    return m_scheduler->readyQueue().enqueue(process, outError);
}

bool ApplicationService::dequeueProcess(Models::Process *outProcess, QString *outError)
{
    if (!m_scheduler) {
        if (outError) *outError = QStringLiteral("Scheduler service is not initialized.");
        return false;
    }
    return m_scheduler->readyQueue().dequeue(outProcess, outError);
}

bool ApplicationService::updateProcessState(int flightId,
                                            Models::ProcessState newState,
                                            QString *outError)
{
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot update process states.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:ManageScheduling"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to update process states (ATC/Admin only).")
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

    // 1. Fetch current process from database to evaluate current state
    Models::Process currentProc;
    if (!getProcessByFlightId(flightId, currentProc, outError)) {
        return false;
    }

    // 2. Validate state transition according to process lifecycle rules
    if (!currentProc.canTransitionTo(newState, outError)) {
        return false;
    }

    // 3. Update database record via parameterized query
    QSqlQuery updateQuery(db);
    if (newState == Models::ProcessState::Completed) {
        updateQuery.prepare(QStringLiteral(
            "UPDATE scheduling_queue "
            "SET process_state = :state, completion_time = CURRENT_TIMESTAMP "
            "WHERE flight_id = :flightId;"
        ));
    } else {
        updateQuery.prepare(QStringLiteral(
            "UPDATE scheduling_queue "
            "SET process_state = :state "
            "WHERE flight_id = :flightId;"
        ));
    }
    updateQuery.bindValue(QStringLiteral(":state"), Models::Process::stateToDbString(newState));
    updateQuery.bindValue(QStringLiteral(":flightId"), flightId);

    if (!updateQuery.exec()) {
        qWarning() << "[ApplicationService] updateProcessState query failed:" << updateQuery.lastError().text();
        if (outError) {
            QString dbErr = updateQuery.lastError().databaseText();
            *outError = QStringLiteral("Database update failed: %1")
                            .arg(dbErr.isEmpty() ? updateQuery.lastError().text() : dbErr);
        }
        return false;
    }

    // 4. Update in-memory ReadyQueue to maintain queue-state consistency
    if (m_scheduler) {
        if (newState != Models::ProcessState::Ready) {
            // Process moved out of Ready (e.g. Running, Blocked, Completed); remove from ready queue
            m_scheduler->readyQueue().removeByFlightId(flightId);
        } else {
            // Transitioned to Ready (e.g. from Blocked); add to ready queue if not already present
            if (!m_scheduler->readyQueue().containsFlight(flightId)) {
                currentProc.state = Models::ProcessState::Ready;
                m_scheduler->readyQueue().enqueue(currentProc);
            }
        }
    }

    emit processesChanged();
    return true;
}

bool ApplicationService::syncReadyQueueFromDatabase(QString *outError)
{
    if (!checkDatabaseConnection()) {
        if (outError) {
            *outError = QStringLiteral("Database connection is offline: %1")
                            .arg(DatabaseManager::instance().statusDescription());
        }
        return false;
    }

    if (!m_scheduler) {
        if (outError) *outError = QStringLiteral("Scheduler service is not initialized.");
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        if (outError) *outError = QStringLiteral("Database is not currently open.");
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT q.queue_id, q.flight_id, q.arrival_time, q.base_priority, q.effective_priority, "
        "       q.service_time, q.waiting_time, q.turnaround_time, q.completion_time, q.process_state, "
        "       f.flight_number, f.airline, f.operational_status "
        "FROM scheduling_queue q "
        "LEFT JOIN flights f ON q.flight_id = f.flight_id "
        "WHERE q.process_state = 'Ready' "
        "ORDER BY q.queue_id ASC;"
    ));

    if (!query.exec()) {
        qWarning() << "[ApplicationService] syncReadyQueueFromDatabase query failed:" << query.lastError().text();
        if (outError) *outError = QStringLiteral("Failed to fetch ready processes: %1").arg(query.lastError().text());
        return false;
    }

    m_scheduler->readyQueue().clear();

    while (query.next()) {
        Models::Process proc;
        proc.setQueueId(query.value(QStringLiteral("queue_id")).toInt());
        proc.flightId = query.value(QStringLiteral("flight_id")).toInt();
        proc.arrivalTime = query.value(QStringLiteral("arrival_time")).toDateTime();
        proc.basePriority = query.value(QStringLiteral("base_priority")).toInt();
        proc.effectivePriority = query.value(QStringLiteral("effective_priority")).toInt();
        proc.serviceTime = query.value(QStringLiteral("service_time")).toInt();
        proc.waitingTime = query.value(QStringLiteral("waiting_time")).isNull() ? -1 : query.value(QStringLiteral("waiting_time")).toInt();
        proc.turnaroundTime = query.value(QStringLiteral("turnaround_time")).isNull() ? -1 : query.value(QStringLiteral("turnaround_time")).toInt();
        proc.completionTime = query.value(QStringLiteral("completion_time")).isNull() ? QDateTime() : query.value(QStringLiteral("completion_time")).toDateTime();
        proc.state = Models::ProcessState::Ready;
        proc.flightNumber = query.value(QStringLiteral("flight_number")).toString();
        proc.airline = query.value(QStringLiteral("airline")).toString();
        proc.operationalStatus = query.value(QStringLiteral("operational_status")).toString();

        m_scheduler->readyQueue().enqueue(proc);
    }

    return true;
}

bool ApplicationService::dispatchNextProcess(Models::Process *outDispatched, QString *outError)
{
    // 1. RBAC check
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot dispatch processes.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:ManageScheduling"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to dispatch scheduling processes.")
                            .arg(m_session.role());
        }
        return false;
    }

    if (!m_scheduler) {
        if (outError) *outError = QStringLiteral("Scheduler service is not initialized.");
        return false;
    }

    // 2. Non-preemptive check
    if (m_scheduler->hasRunningProcess()) {
        if (outError) {
            *outError = QStringLiteral("Non-preemptive scheduler busy: process for flight %1 is currently RUNNING.")
                            .arg(m_scheduler->runningProcess().flightId);
        }
        return false;
    }

    // 3. Select next process candidate
    Models::Process candidate;
    QString selectReason;
    if (!m_scheduler->selectNextProcess(&candidate, &selectReason)) {
        if (outError) *outError = selectReason;
        return false;
    }

    // 4. Validate transition to Running
    QString transReason;
    if (!candidate.canTransitionTo(Models::ProcessState::Running, &transReason)) {
        if (outError) *outError = QStringLiteral("Cannot dispatch flight %1: %2").arg(candidate.flightId).arg(transReason);
        return false;
    }

    // 5. Database update if DB is connected
    if (DatabaseManager::instance().isConnected()) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "UPDATE scheduling_queue "
                "SET process_state = 'Running' "
                "WHERE flight_id = :fid AND process_state = 'Ready';"
            ));
            q.bindValue(QStringLiteral(":fid"), candidate.flightId);

            if (!q.exec()) {
                qWarning() << "[ApplicationService] dispatchNextProcess DB update failed:" << q.lastError().text();
                if (outError) {
                    *outError = QStringLiteral("Database update failed during dispatch: %1").arg(q.lastError().text());
                }
                return false;
            }

            if (q.numRowsAffected() == 0) {
                if (outError) {
                    *outError = QStringLiteral("Database update failed: process for flight %1 was not in 'Ready' state in database.")
                                    .arg(candidate.flightId);
                }
                return false;
            }
        }
    }

    // 6. Dispatch in Scheduler (removes candidate from ReadyQueue and marks Running)
    Scheduler::DispatchResult res = m_scheduler->dispatchNextProcess();
    if (!res.success) {
        if (outError) *outError = res.reason;
        return false;
    }

    if (outDispatched) {
        *outDispatched = res.process;
    }

    emit processesChanged();
    return true;
}

bool ApplicationService::completeRunningProcess(const QDateTime &completedAt,
                                                Models::Process *outCompleted,
                                                QString *outError)
{
    // 1. RBAC check
    if (!isAuthenticated()) {
        if (outError) *outError = QStringLiteral("Authentication required: unauthenticated users cannot complete processes.");
        return false;
    }

    if (!m_session.hasPermission(QStringLiteral("action:ManageScheduling"))) {
        if (outError) {
            *outError = QStringLiteral("Access denied: role '%1' is not authorized to complete scheduling processes.")
                            .arg(m_session.role());
        }
        return false;
    }

    if (!m_scheduler) {
        if (outError) *outError = QStringLiteral("Scheduler service is not initialized.");
        return false;
    }

    if (!m_scheduler->hasRunningProcess()) {
        if (outError) *outError = QStringLiteral("No process is currently RUNNING to complete.");
        return false;
    }

    Models::Process running = m_scheduler->runningProcess();

    // Determine completion timestamp and compute metrics
    QDateTime compTime = completedAt.isValid() ? completedAt : running.arrivalTime.addSecs(running.serviceTime);
    QString metricErr;
    if (!running.calculateMetrics(compTime, &metricErr)) {
        if (outError) *outError = metricErr;
        return false;
    }

    // 2. Database update if DB is connected
    if (DatabaseManager::instance().isConnected()) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "UPDATE scheduling_queue "
                "SET process_state = 'Completed', "
                "    waiting_time = :wt, "
                "    turnaround_time = :tat, "
                "    completion_time = :ct "
                "WHERE flight_id = :fid AND process_state = 'Running';"
            ));
            q.bindValue(QStringLiteral(":wt"), running.waitingTime);
            q.bindValue(QStringLiteral(":tat"), running.turnaroundTime);
            q.bindValue(QStringLiteral(":ct"), running.completionTime);
            q.bindValue(QStringLiteral(":fid"), running.flightId);

            if (!q.exec()) {
                qWarning() << "[ApplicationService] completeRunningProcess DB update failed:" << q.lastError().text();
                if (outError) {
                    *outError = QStringLiteral("Database update failed during process completion: %1").arg(q.lastError().text());
                }
                return false;
            }

            if (q.numRowsAffected() == 0) {
                if (outError) {
                    *outError = QStringLiteral("Database update failed: process for flight %1 was not in 'Running' state in database.")
                                    .arg(running.flightId);
                }
                return false;
            }
        }
    }

    // 3. Complete in Scheduler (computes metrics, records in m_completedProcesses, clears running)
    Scheduler::CompletionResult res = m_scheduler->completeRunningProcess(compTime);
    if (!res.success) {
        if (outError) *outError = res.reason;
        return false;
    }

    if (outCompleted) {
        *outCompleted = res.process;
    }

    emit processesChanged();
    return true;
}

Scheduler::PerformanceMetrics ApplicationService::getAggregateSchedulingMetrics() const
{
    if (m_scheduler) {
        return m_scheduler->calculateAggregateMetrics();
    }
    return Scheduler::PerformanceMetrics();
}

QList<Models::Process> ApplicationService::getCompletedProcesses() const
{
    if (m_scheduler) {
        return m_scheduler->completedProcesses();
    }
    return QList<Models::Process>();
}





