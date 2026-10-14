#pragma once

#include <QObject>
#include <QString>

#include "models/Session.h"

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

private:
    Scheduler       *m_scheduler       = nullptr;
    ResourceManager *m_resourceManager = nullptr;
    Session          m_session;
};

