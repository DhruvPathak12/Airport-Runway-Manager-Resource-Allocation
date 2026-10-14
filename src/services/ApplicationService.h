#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QPair>

#include "models/Session.h"
#include "models/Flight.h"

class Scheduler;
class ResourceManager;

/**
 * @brief Central application service coordinator.
 *
 * ApplicationService is the bridge between the GUI layer and all
 * lower-level service/OS/DBMS layers. The UI never calls the database
 * or scheduler directly — it goes through this coordinator.
 *
 * Architecture:
 *
 *   Qt 6 GUI (ui/)
 *       ↓
 *   ApplicationService          ← You are here
 *       ↓              ↓
 *   Scheduler      ResourceManager
 *       ↓              ↓
 *   DatabaseManager (Qt SQL / MySQL)
 *
 * Day 2 scope: Ownership of sub-services established, interface defined.
 * Day 6+: Auth and session management.
 * Day 7+: Flight and aircraft services.
 * Day 11+: Scheduler coordination.
 * Day 14+: Runway resource coordination.
 */
class ApplicationService : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationService(QObject *parent = nullptr);
    ~ApplicationService() override = default;

    /**
     * @brief Initializes the application — connects to DB, loads config.
     * Called once from main() after QApplication is created.
     * Implementation: Day 5 (database init), Day 6 (auth).
     */
    bool initialize();

    /**
     * @brief Clean shutdown — disconnects from DB, releases resources.
     * Called before QApplication exits.
     */
    void shutdown();

    /**
     * @brief Checks database connection status and attempts connection if configured.
     * Safe to call without crashing if DB is unavailable.
     * @return true if database connection is active and responsive.
     */
    bool checkDatabaseConnection();

    /**
     * @brief Authenticates a user against the database.
     * @param username  Supplied username.
     * @param password  Supplied plaintext password (verified then discarded).
     * @param outError  Optional output receiving a sanitized error message on failure.
     * @return true if credentials and status are valid and a session was created.
     */
    bool login(const QString &username, const QString &password, QString *outError = nullptr);

    /**
     * @brief Logs out the currently authenticated user and clears the session.
     */
    void logout();

    /**
     * @brief Returns the active authenticated session.
     */
    const Session& currentSession() const { return m_session; }

    /**
     * @brief Returns true if an authenticated session currently exists.
     */
    bool isAuthenticated() const { return m_session.isValid(); }

    /**
     * @brief Injects an authenticated session for unit testing without live MySQL.
     */
    void setTestSession(const Session &session) { m_session = session; }

    // ── Sub-service accessors ─────────────────────────────────────────────────
    Scheduler       *scheduler()       const { return m_scheduler; }
    ResourceManager *resourceManager() const { return m_resourceManager; }

    // ── Role-Enforced Action Boundary Methods ─────────────────────────────────
    /**
     * @brief Triggers a scheduling cycle with action-level RBAC enforcement.
     * Allowed only for ATC role (or Admin for manual override).
     */
    bool executeSchedulerCycle(QString *outError = nullptr);

    /**
     * @brief Triggers runway allocation with action-level RBAC enforcement.
     * Allowed only for ATC role.
     */
    bool allocateRunwayResource(int runwayId, int flightId, QString *outError = nullptr);

    /**
     * @brief Sets runway maintenance status with action-level RBAC enforcement.
     * Allowed only for Airport Operator role.
     */
    bool setRunwayMaintenanceStatus(int runwayId, bool inMaintenance, QString *outError = nullptr);

    // ── Flight Management Methods (Day 7) ────────────────────────────────────
    /**
     * @brief Fetches all flights, optionally filtered by search text and status.
     * Allowed for: Admin, ATC, Airport Operator (page:Flights).
     */
    QList<Models::Flight> getAllFlights(const QString &searchFilter = QString(),
                                       const QString &statusFilter = QString(),
                                       QString *outError = nullptr);

    /**
     * @brief Fetches an individual flight by ID.
     * Allowed for: Admin, ATC, Airport Operator (page:Flights).
     */
    bool getFlightById(int flightId, Models::Flight &outFlight, QString *outError = nullptr);

    /**
     * @brief Creates a new flight record in MySQL with full validation.
     * Allowed only for ATC role (action:ManageFlights).
     */
    bool createFlight(const Models::Flight &flight, QString *outError = nullptr);

    /**
     * @brief Updates an existing flight record in MySQL with full validation.
     * Allowed only for ATC role (action:ManageFlights).
     */
    bool updateFlight(const Models::Flight &flight, QString *outError = nullptr);

    /**
     * @brief Safely deletes a flight record if no referential constraints block it.
     * Allowed only for ATC role (action:ManageFlights).
     */
    bool deleteFlight(int flightId, QString *outError = nullptr);

    /**
     * @brief Checks if a flight can be deleted safely without foreign-key conflict.
     * Returns true if safe; false with an explanatory reason if related records exist.
     */
    bool canDeleteFlight(int flightId, QString *outReason = nullptr);

    /**
     * @brief Returns a list of available aircraft for flight assignment.
     * Returns pairs of (aircraft_id, "VT-AT1 — Boeing 737-800 [Active]").
     */
    QList<QPair<int, QString>> getAvailableAircraft(QString *outError = nullptr);

signals:
    /**
     * @brief Emitted when the application is fully initialized and ready.
     */
    void applicationReady();

    /**
     * @brief Emitted when a user successfully authenticates.
     */
    void userLoggedIn(const Session &session);

    /**
     * @brief Emitted when a user logs out.
     */
    void userLoggedOut();

    /**
     * @brief Emitted when a fatal error occurs during initialization.
     */
    void initializationFailed(const QString &reason);

    /**
     * @brief Emitted when flight records are created, updated, or deleted.
     */
    void flightsChanged();

private:
    Scheduler       *m_scheduler       = nullptr;
    ResourceManager *m_resourceManager = nullptr;
    Session          m_session;
};

