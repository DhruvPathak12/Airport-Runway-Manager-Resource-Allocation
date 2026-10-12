#pragma once

#include <QString>
#include <QMap>

/**
 * @brief Manages the Qt SQL database connection.
 *
 * This is the ONLY class that interacts directly with the database connection.
 * All other data-access classes go through this class.
 *
 * Day 2 scope: Foundation + interface only.
 * Day 5: Actual Qt SQL / QMYSQL connection implementation.
 *
 * DBMS Concepts demonstrated (Phase 1+2):
 *   - Non-hardcoded credentials (loaded from db.conf)
 *   - Single named connection (Qt SQL connection management)
 *   - ACID transactions via QSqlDatabase::transaction() / commit() / rollback()
 */
class DatabaseManager
{
public:
    /**
     * @brief Returns the singleton instance.
     */
    static DatabaseManager& instance();

    /**
     * @brief Loads database credentials from a config file.
     * @param configFilePath  Path to db.conf (never hardcode credentials).
     * @return true if all required keys were loaded.
     *
     * Implementation: Day 5.
     * Expected keys: host, port, dbname, user, password
     */
    bool loadConfig(const QString &configFilePath);

    /**
     * @brief Opens the MySQL database connection using loaded credentials.
     * @return true if connection succeeded.
     *
     * Implementation: Day 5.
     * Uses Qt SQL QMYSQL driver.
     */
    bool connect();

    /**
     * @brief Closes the database connection cleanly.
     *
     * Implementation: Day 5.
     */
    void disconnect();

    /**
     * @brief Returns true if a valid open connection exists.
     */
    bool isConnected() const;

    /**
     * @brief Returns the last database error message, if any.
     */
    QString lastError() const;

private:
    DatabaseManager();
    ~DatabaseManager();

    // Non-copyable singleton
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    // Internal state
    QMap<QString, QString> m_config;
    bool                   m_connected = false;
    QString                m_lastError;
};
