#include "ApplicationService.h"

#include "scheduler/Scheduler.h"
#include "resource/ResourceManager.h"
#include "database/DatabaseManager.h"

#include <QDebug>

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

void ApplicationService::shutdown()
{
    qDebug() << "[ApplicationService] Shutting down...";

    // Day 5: Disconnect cleanly from database
    DatabaseManager::instance().disconnect();

    qDebug() << "[ApplicationService] Shutdown complete.";
}

