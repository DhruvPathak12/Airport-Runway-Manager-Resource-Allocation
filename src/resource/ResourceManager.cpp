#include "ResourceManager.h"

#include <QDebug>

ResourceManager::ResourceManager(QObject *parent)
    : QObject(parent)
{
}

bool ResourceManager::allocateRunway(int runwayId, int flightId)
{
    // TODO (Day 14): Basic runway allocation.
    // TODO (Day 19): Wrap in std::mutex critical section.
    // TODO (Day 21): Wrap in ACID transaction (BEGIN / SELECT FOR UPDATE / COMMIT).
    Q_UNUSED(runwayId)
    Q_UNUSED(flightId)
    qDebug() << "[ResourceManager] allocateRunway() — stub (implemented Day 14)";
    return false;
}

bool ResourceManager::releaseRunway(int runwayId)
{
    // TODO (Day 14): Release runway, update DB status to 'Available'.
    Q_UNUSED(runwayId)
    qDebug() << "[ResourceManager] releaseRunway() — stub (implemented Day 14)";
    return false;
}

QString ResourceManager::runwayStatus(int runwayId) const
{
    // TODO (Day 14): Query runway status from database.
    Q_UNUSED(runwayId)
    return "Unknown";
}
