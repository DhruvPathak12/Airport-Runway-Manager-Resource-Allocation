#include "ResourceManager.h"
#include "database/DatabaseManager.h"

#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

ResourceManager::ResourceManager(QObject *parent)
    : QObject(parent)
{
}

bool ResourceManager::allocateRunway(int runwayId, int flightId)
{
    // High-level allocation logic and business constraints are coordinated by
    // ApplicationService::allocateRunway(runwayId, flightId).
    // Mutex critical section will be added here in Day 19.
    // ACID transaction boundary will be added in Day 21.
    Q_UNUSED(runwayId)
    Q_UNUSED(flightId)
    return true;
}

bool ResourceManager::releaseRunway(int runwayId)
{
    // High-level runway release logic and business constraints are coordinated by
    // ApplicationService::releaseRunway(runwayId).
    Q_UNUSED(runwayId)
    return true;
}

QString ResourceManager::runwayStatus(int runwayId) const
{
    if (runwayId <= 0) {
        return QStringLiteral("Unknown");
    }

    if (!DatabaseManager::instance().isConnected()) {
        return QStringLiteral("Unknown");
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        return QStringLiteral("Unknown");
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :id;"));
    query.bindValue(QStringLiteral(":id"), runwayId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    return QStringLiteral("Unknown");
}
