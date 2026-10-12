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

    // TODO (Day 5): Load db.conf and open MySQL connection via DatabaseManager.
    // TODO (Day 6): Validate session / show login if no active session.

    qDebug() << "[ApplicationService] Initialization stub complete (Day 2).";
    return true;
}

void ApplicationService::shutdown()
{
    qDebug() << "[ApplicationService] Shutting down...";

    // TODO (Day 5): DatabaseManager::instance().disconnect()

    qDebug() << "[ApplicationService] Shutdown complete.";
}
