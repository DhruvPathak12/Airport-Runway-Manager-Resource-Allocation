#pragma once

#include <QString>
#include <QMap>
#include <QSqlDatabase>

/**
 * @brief Manages the Qt SQL database connection for SmartATC.
 *
 * This is the ONLY class that interacts directly with the Qt SQL database connection.
 * All other data-access classes access the database through this manager.
 *
 * Day 5 implementation:
 *   - Non-hardcoded credentials loaded from db.conf
 *   - INI / Key-Value configuration parsing and validation
 *   - Explicit QMYSQL driver check and reporting
 *   - Single named connection ("SmartATCConnection") lifecycle management
 *   - Sanitized error reporting (credentials never exposed)
 *   - Read-only connectivity verification (SELECT VERSION(), SELECT DATABASE())
 */
class DatabaseManager
{
public:
    /**
     * @brief Returns the singleton instance.
     */
    static DatabaseManager& instance();

    /**
     * @brief Resolves the default configuration file path.
     * Looks at application directory, working directory, and project root.
     */
    static QString defaultConfigFile();

    /**
     * @brief Checks if the Qt SQL QMYSQL driver plugin is available.
     */
    static bool isDriverAvailable();

    /**
     * @brief Loads and validates database credentials from a config file.
     * @param configFilePath  Path to db.conf (defaults to defaultConfigFile() if empty).
     * @return true if all required keys were loaded and valid.
     */
    bool loadConfig(const QString &configFilePath = QString());

    /**
     * @brief Opens the MySQL database connection using loaded credentials.
     * @return true if connection succeeded.
     */
    bool connect();

    /**
     * @brief Closes the database connection cleanly.
     */
    void disconnect();

    /**
     * @brief Returns true if a valid open connection exists.
     */
    bool isConnected() const;

    /**
     * @brief Returns the last database error message, sanitized against credentials.
     */
    QString lastError() const;

    /**
     * @brief Executes a minimal read-only connectivity query (e.g. SELECT VERSION(), SELECT DATABASE()).
     * Does NOT execute any state-modifying queries (INSERT, UPDATE, DELETE, etc.).
     * @param outVersion   Output parameter receiving server version string if successful.
     * @param outDatabase  Output parameter receiving active database name if successful.
     * @return true if query succeeded.
     */
    bool testConnectivity(QString *outVersion = nullptr, QString *outDatabase = nullptr);

    /**
     * @brief Detailed connection status enumeration.
     */
    enum class ConnectionState {
        NotConfigured,
        DriverUnavailable,
        ServerUnreachable,
        AuthFailed,
        DatabaseNotFound,
        Connected,
        Disconnected
    };

    /**
     * @brief Returns the current connection state.
     */
    ConnectionState connectionState() const;

    /**
     * @brief Returns a user-facing descriptive summary of the connection state.
     */
    QString statusDescription() const;

    /**
     * @brief Returns configured database parameters (excluding password).
     */
    QString host() const;
    int     port() const;
    QString databaseName() const;
    QString username() const;

    /**
     * @brief Named connection identifier used across Qt SQL.
     */
    static const QString CONNECTION_NAME;

private:
    DatabaseManager();
    ~DatabaseManager();

    // Non-copyable singleton
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    void setSanitizedError(const QString &rawError);

    // Internal state
    QMap<QString, QString> m_config;
    bool                   m_connected = false;
    ConnectionState        m_state     = ConnectionState::NotConfigured;
    QString                m_lastError;
    QString                m_configFilePath;
};

