#include "ResourceManager.h"
#include "database/DatabaseManager.h"

#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

// ── ScopedResourceLock RAII Implementation ───────────────────────────────────

ResourceManager::ScopedResourceLock::ScopedResourceLock(std::mutex &mtx, int runwayId, ResourceManager *mgr)
    : m_lock(mtx)
    , m_runwayId(runwayId)
    , m_mgr(mgr)
{
    if (m_mgr && m_runwayId > 0) {
        m_mgr->setCriticalSectionStatusUnlocked(m_runwayId, QStringLiteral("Critical Section Entered"));
        emit m_mgr->criticalSectionEntered(m_runwayId, 0);
    }
}

ResourceManager::ScopedResourceLock::~ScopedResourceLock()
{
    if (m_lock.owns_lock()) {
        if (m_mgr && m_runwayId > 0) {
            m_mgr->setCriticalSectionStatusUnlocked(m_runwayId, QStringLiteral("Critical Section Released"));
            emit m_mgr->criticalSectionExited(m_runwayId, QStringLiteral("Critical Section Released"));
        }
        m_lock.unlock();
    }
}

ResourceManager::ScopedResourceLock::ScopedResourceLock(ScopedResourceLock &&other) noexcept
    : m_lock(std::move(other.m_lock))
    , m_runwayId(other.m_runwayId)
    , m_mgr(other.m_mgr)
{
    other.m_runwayId = 0;
    other.m_mgr = nullptr;
}

ResourceManager::ScopedResourceLock &ResourceManager::ScopedResourceLock::operator=(ScopedResourceLock &&other) noexcept
{
    if (this != &other) {
        if (m_lock.owns_lock()) {
            if (m_mgr && m_runwayId > 0) {
                m_mgr->setCriticalSectionStatusUnlocked(m_runwayId, QStringLiteral("Critical Section Released"));
                emit m_mgr->criticalSectionExited(m_runwayId, QStringLiteral("Critical Section Released"));
            }
            m_lock.unlock();
        }
        m_lock = std::move(other.m_lock);
        m_runwayId = other.m_runwayId;
        m_mgr = other.m_mgr;
        other.m_runwayId = 0;
        other.m_mgr = nullptr;
    }
    return *this;
}

void ResourceManager::ScopedResourceLock::unlock()
{
    if (m_lock.owns_lock()) {
        if (m_mgr && m_runwayId > 0) {
            m_mgr->setCriticalSectionStatusUnlocked(m_runwayId, QStringLiteral("Critical Section Released"));
            emit m_mgr->criticalSectionExited(m_runwayId, QStringLiteral("Critical Section Released"));
        }
        m_lock.unlock();
    }
}

void ResourceManager::ScopedResourceLock::lock()
{
    if (!m_lock.owns_lock()) {
        m_lock.lock();
        if (m_mgr && m_runwayId > 0) {
            m_mgr->setCriticalSectionStatusUnlocked(m_runwayId, QStringLiteral("Critical Section Entered"));
            emit m_mgr->criticalSectionEntered(m_runwayId, 0);
        }
    }
}

// ── ResourceManager Implementation ───────────────────────────────────────────

ResourceManager::ResourceManager(QObject *parent)
    : QObject(parent)
{
}

// ── Unlocked Helpers (Caller holds m_mutex) ──────────────────────────────────

bool ResourceManager::isRunwayReservedUnlocked(int runwayId) const
{
    return m_activeAllocations.contains(runwayId);
}

int ResourceManager::activeFlightForRunwayUnlocked(int runwayId) const
{
    return m_activeAllocations.value(runwayId, 0);
}

int ResourceManager::activeRunwayForFlightUnlocked(int flightId) const
{
    for (auto it = m_activeAllocations.cbegin(); it != m_activeAllocations.cend(); ++it) {
        if (it.value() == flightId) {
            return it.key();
        }
    }
    return 0;
}

bool ResourceManager::isRunwayInMaintenanceUnlocked(int runwayId) const
{
    return m_maintenanceRunways.contains(runwayId);
}

void ResourceManager::setRunwayMaintenanceUnlocked(int runwayId, bool inMaintenance)
{
    if (inMaintenance) {
        m_maintenanceRunways.insert(runwayId);
        m_csStatus[runwayId] = QStringLiteral("Resource Busy");
        emit runwayStatusChanged(runwayId, QStringLiteral("Maintenance"));
    } else {
        m_maintenanceRunways.remove(runwayId);
        if (m_activeAllocations.contains(runwayId)) {
            m_csStatus[runwayId] = QStringLiteral("Resource Reserved");
            emit runwayStatusChanged(runwayId, QStringLiteral("Occupied"));
        } else {
            m_csStatus[runwayId] = QStringLiteral("Resource Available");
            emit runwayStatusChanged(runwayId, QStringLiteral("Available"));
        }
    }
}

QString ResourceManager::criticalSectionStatusUnlocked(int runwayId) const
{
    if (m_csStatus.contains(runwayId)) {
        return m_csStatus.value(runwayId);
    }
    if (m_maintenanceRunways.contains(runwayId)) {
        return QStringLiteral("Resource Busy");
    }
    if (m_activeAllocations.contains(runwayId)) {
        return QStringLiteral("Resource Reserved");
    }
    return QStringLiteral("Resource Available");
}

void ResourceManager::setCriticalSectionStatusUnlocked(int runwayId, const QString &status)
{
    m_csStatus[runwayId] = status;
}

void ResourceManager::rollbackAllocationUnlocked(int runwayId, int flightId)
{
    if (m_activeAllocations.value(runwayId) == flightId) {
        m_activeAllocations.remove(runwayId);
    }
    m_csStatus[runwayId] = QStringLiteral("Resource Available");
    emit runwayStatusChanged(runwayId, QStringLiteral("Available"));
    emit criticalSectionExited(runwayId, QStringLiteral("Critical Section Released"));
}

void ResourceManager::confirmAllocationUnlocked(int runwayId, int flightId)
{
    m_activeAllocations.insert(runwayId, flightId);
    m_csStatus[runwayId] = QStringLiteral("Resource Reserved");
    emit runwayReserved(runwayId, flightId);
    emit runwayStatusChanged(runwayId, QStringLiteral("Occupied"));
    emit criticalSectionExited(runwayId, QStringLiteral("Resource Reserved"));
}

bool ResourceManager::releaseRunwayUnlocked(int runwayId, QString *outError)
{
    if (runwayId <= 0) {
        if (outError) {
            *outError = QStringLiteral("Invalid runway ID: %1. Must be greater than zero.").arg(runwayId);
        }
        m_csStatus[runwayId] = QStringLiteral("Allocation Rejected");
        emit criticalSectionExited(runwayId, QStringLiteral("Allocation Rejected"));
        return false;
    }

    if (!m_activeAllocations.contains(runwayId)) {
        bool foundInDb = false;
        if (DatabaseManager::instance().isConnected()) {
            QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
            if (db.isOpen()) {
                QSqlQuery q(db);
                q.prepare(QStringLiteral("SELECT assignment_id FROM runway_assignments WHERE runway_id = :rid AND status = 'Active';"));
                q.bindValue(QStringLiteral(":rid"), runwayId);
                if (q.exec() && q.next()) {
                    foundInDb = true;
                }
            }
        }

        if (!foundInDb) {
            if (outError) {
                *outError = QStringLiteral("Runway %1 has no active assignment to release.").arg(runwayId);
            }
            m_csStatus[runwayId] = QStringLiteral("Allocation Rejected");
            emit criticalSectionExited(runwayId, QStringLiteral("Allocation Rejected"));
            return false;
        }
    } else {
        m_activeAllocations.remove(runwayId);
    }

    m_csStatus[runwayId] = QStringLiteral("Resource Available");
    emit runwayReleased(runwayId);
    emit runwayStatusChanged(runwayId, QStringLiteral("Available"));
    emit criticalSectionExited(runwayId, QStringLiteral("Critical Section Released"));
    return true;
}

// ── Locked Public Methods ───────────────────────────────────────────────────

bool ResourceManager::allocateRunway(int runwayId, int flightId, QString *outError)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_concurrentCount++;
    if (m_concurrentCount > m_maxConcurrentObserved) {
        m_maxConcurrentObserved = m_concurrentCount;
    }

    struct ExitGuard {
        int &count;
        ~ExitGuard() { count--; }
    } exitGuard{m_concurrentCount};

    emit criticalSectionEntered(runwayId, flightId);
    m_csStatus[runwayId] = QStringLiteral("Critical Section Entered");

    if (runwayId <= 0 || flightId <= 0) {
        if (outError) {
            *outError = QStringLiteral("Invalid input: runway ID and flight ID must be positive.");
        }
        m_csStatus[runwayId] = QStringLiteral("Allocation Rejected");
        emit allocationFailed(runwayId, QStringLiteral("Invalid runway or flight ID"));
        emit criticalSectionExited(runwayId, QStringLiteral("Allocation Rejected"));
        return false;
    }

    // 1. Invariant: Runway must not be in maintenance
    if (m_maintenanceRunways.contains(runwayId)) {
        QString err = QStringLiteral("Runway %1 is currently in Maintenance and cannot be allocated.").arg(runwayId);
        if (outError) {
            *outError = err;
        }
        m_csStatus[runwayId] = QStringLiteral("Allocation Rejected");
        emit allocationFailed(runwayId, err);
        emit criticalSectionExited(runwayId, QStringLiteral("Allocation Rejected"));
        return false;
    }

    // 2. Invariant: Runway must not already have an active allocation (Mutual Exclusion)
    if (m_activeAllocations.contains(runwayId)) {
        int existingFlightId = m_activeAllocations.value(runwayId);
        QString err = QStringLiteral("Resource Contention: Runway %1 already allocated to flight %2.")
                          .arg(runwayId).arg(existingFlightId);
        if (outError) {
            *outError = err;
        }
        m_csStatus[runwayId] = QStringLiteral("Resource Busy");
        emit allocationContentionRejected(runwayId, flightId, err);
        emit allocationFailed(runwayId, err);
        emit criticalSectionExited(runwayId, QStringLiteral("Allocation Rejected"));
        return false;
    }

    // 3. Invariant: Flight must not already hold an active runway
    for (auto it = m_activeAllocations.cbegin(); it != m_activeAllocations.cend(); ++it) {
        if (it.value() == flightId) {
            QString err = QStringLiteral("Flight %1 already holds an active allocation on runway %2.")
                              .arg(flightId).arg(it.key());
            if (outError) {
                *outError = err;
            }
            m_csStatus[runwayId] = QStringLiteral("Allocation Rejected");
            emit allocationFailed(runwayId, err);
            emit criticalSectionExited(runwayId, QStringLiteral("Allocation Rejected"));
            return false;
        }
    }

    // Reserve resource
    m_activeAllocations.insert(runwayId, flightId);
    m_csStatus[runwayId] = QStringLiteral("Resource Reserved");

    emit runwayReserved(runwayId, flightId);
    emit runwayStatusChanged(runwayId, QStringLiteral("Occupied"));
    emit criticalSectionExited(runwayId, QStringLiteral("Resource Reserved"));
    return true;
}

bool ResourceManager::releaseRunway(int runwayId, QString *outError)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_concurrentCount++;
    if (m_concurrentCount > m_maxConcurrentObserved) {
        m_maxConcurrentObserved = m_concurrentCount;
    }

    struct ExitGuard {
        int &count;
        ~ExitGuard() { count--; }
    } exitGuard{m_concurrentCount};

    emit criticalSectionEntered(runwayId, 0);
    m_csStatus[runwayId] = QStringLiteral("Critical Section Entered");

    return releaseRunwayUnlocked(runwayId, outError);
}

void ResourceManager::rollbackAllocation(int runwayId, int flightId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    rollbackAllocationUnlocked(runwayId, flightId);
}

void ResourceManager::confirmAllocation(int runwayId, int flightId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    confirmAllocationUnlocked(runwayId, flightId);
}

QString ResourceManager::runwayStatus(int runwayId) const
{
    if (runwayId <= 0) {
        return QStringLiteral("Unknown");
    }

    if (DatabaseManager::instance().isConnected()) {
        QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
        if (db.isOpen()) {
            QSqlQuery query(db);
            query.prepare(QStringLiteral("SELECT status FROM runways WHERE runway_id = :id;"));
            query.bindValue(QStringLiteral(":id"), runwayId);

            if (query.exec() && query.next()) {
                return query.value(0).toString();
            }
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_maintenanceRunways.contains(runwayId)) {
        return QStringLiteral("Maintenance");
    }
    if (m_activeAllocations.contains(runwayId)) {
        return QStringLiteral("Occupied");
    }
    return QStringLiteral("Available");
}

bool ResourceManager::isRunwayReserved(int runwayId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return isRunwayReservedUnlocked(runwayId);
}

int ResourceManager::activeFlightForRunway(int runwayId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return activeFlightForRunwayUnlocked(runwayId);
}

int ResourceManager::activeRunwayForFlight(int flightId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return activeRunwayForFlightUnlocked(flightId);
}

void ResourceManager::setRunwayMaintenance(int runwayId, bool inMaintenance)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setRunwayMaintenanceUnlocked(runwayId, inMaintenance);
}

bool ResourceManager::isRunwayInMaintenance(int runwayId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return isRunwayInMaintenanceUnlocked(runwayId);
}

QString ResourceManager::criticalSectionStatus(int runwayId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return criticalSectionStatusUnlocked(runwayId);
}

void ResourceManager::setCriticalSectionStatus(int runwayId, const QString &status)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setCriticalSectionStatusUnlocked(runwayId, status);
}

bool ResourceManager::tryEnterCriticalSection(int runwayId)
{
    if (m_mutex.try_lock()) {
        m_concurrentCount++;
        if (m_concurrentCount > m_maxConcurrentObserved) {
            m_maxConcurrentObserved = m_concurrentCount;
        }
        if (runwayId > 0) {
            m_csStatus[runwayId] = QStringLiteral("Critical Section Entered");
            emit criticalSectionEntered(runwayId, 0);
        }
        return true;
    }
    return false;
}

void ResourceManager::exitCriticalSection(int runwayId, const QString &finalStatus)
{
    if (runwayId > 0) {
        m_csStatus[runwayId] = finalStatus;
        emit criticalSectionExited(runwayId, finalStatus);
    }
    m_concurrentCount--;
    m_mutex.unlock();
}

void ResourceManager::syncFromDatabase()
{
    if (!DatabaseManager::instance().isConnected()) {
        return;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    if (!db.isOpen()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeAllocations.clear();
    m_maintenanceRunways.clear();

    // Load active assignments
    QSqlQuery assignQuery(db);
    if (assignQuery.exec(QStringLiteral("SELECT runway_id, flight_id FROM runway_assignments WHERE status = 'Active';"))) {
        while (assignQuery.next()) {
            int rId = assignQuery.value(0).toInt();
            int fId = assignQuery.value(1).toInt();
            m_activeAllocations.insert(rId, fId);
            m_csStatus[rId] = QStringLiteral("Resource Reserved");
        }
    }

    // Load maintenance runways
    QSqlQuery maintQuery(db);
    if (maintQuery.exec(QStringLiteral("SELECT runway_id FROM runways WHERE status = 'Maintenance';"))) {
        while (maintQuery.next()) {
            int rId = maintQuery.value(0).toInt();
            m_maintenanceRunways.insert(rId);
            m_csStatus[rId] = QStringLiteral("Resource Busy");
        }
    }
}

void ResourceManager::clearState()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeAllocations.clear();
    m_maintenanceRunways.clear();
    m_csStatus.clear();
    m_concurrentCount = 0;
    m_maxConcurrentObserved = 0;
}
