#include "DatabaseManager.h"

#include <QDebug>

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
// Public interface — Stub implementations (completed Day 5)
// ─────────────────────────────────────────────────────────────────────────────

bool DatabaseManager::loadConfig(const QString &configFilePath)
{
    // TODO (Day 5): Parse db.conf key=value pairs into m_config.
    // Required keys: host, port, dbname, user, password
    Q_UNUSED(configFilePath)
    qDebug() << "[DatabaseManager] loadConfig() — stub (implemented Day 5)";
    return false;
}

bool DatabaseManager::connect()
{
    // TODO (Day 5): Open QMYSQL connection using m_config values.
    // Use a named Qt SQL connection: "SmartATCConnection"
    qDebug() << "[DatabaseManager] connect() — stub (implemented Day 5)";
    return false;
}

void DatabaseManager::disconnect()
{
    // TODO (Day 5): Close and remove the named Qt SQL connection.
    if (m_connected) {
        qDebug() << "[DatabaseManager] disconnect() — stub (implemented Day 5)";
        m_connected = false;
    }
}

bool DatabaseManager::isConnected() const
{
    return m_connected;
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}
