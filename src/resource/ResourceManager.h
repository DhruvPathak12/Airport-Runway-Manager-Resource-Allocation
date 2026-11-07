#pragma once

#include <QObject>
#include <QMap>
#include <QSet>
#include <QString>
#include <mutex>
#include <memory>

#include "models/Runway.h"

/**
 * @brief Manages shared runway resources and enforces mutual exclusion.
 *
 * ── OS Concept Mapping ────────────────────────────────────────────────────────
 *   A Runway is a shared OS resource (equivalent to an exclusive device/memory).
 *   This class enforces mutual exclusion to protect runway allocation and release
 *   operations from race conditions and concurrent access conflicts.
 *
 *   Runway                     → Shared OS resource
 *   Flight                     → Simulated Process requesting the resource
 *   Critical Section           → Protected code block accessing/mutating runway state
 *   Mutual Exclusion           → Only one thread/operation can modify resource state at a time
 *   RAII Lock                  → std::lock_guard / std::unique_lock ensuring automatic release
 *   Resource Contention        → Multiple flights requesting the same runway
 *
 * ── Phase scope ───────────────────────────────────────────────────────────────
 *   Day 2  : Interface only. No allocation. No mutex. No transactions.
 *   Day 14 : Basic runway management implemented.
 *   Day 15 : Runway resource allocation rules and validation.
 *   Day 19 : Critical Section and Mutual Exclusion (in-process std::mutex + RAII).
 *   Day 20 : Advanced C++ threading, synchronization, and contention tests.
 *   Day 21–22 : ACID transaction boundary wraps allocation in MySQL.
 */
class ResourceManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Scoped RAII guard for the ResourceManager's internal mutex.
     * Guarantees release upon normal exit or when an exception is thrown.
     */
    class ScopedResourceLock
    {
    public:
        explicit ScopedResourceLock(std::mutex &mtx, int runwayId = 0, ResourceManager *mgr = nullptr);
        ~ScopedResourceLock();

        // Non-copyable
        ScopedResourceLock(const ScopedResourceLock &) = delete;
        ScopedResourceLock &operator=(const ScopedResourceLock &) = delete;

        // Movable
        ScopedResourceLock(ScopedResourceLock &&other) noexcept;
        ScopedResourceLock &operator=(ScopedResourceLock &&other) noexcept;

        bool isLocked() const noexcept { return m_lock.owns_lock(); }
        void unlock();
        void lock();

    private:
        std::unique_lock<std::mutex> m_lock;
        int m_runwayId = 0;
        ResourceManager *m_mgr = nullptr;
    };

    explicit ResourceManager(QObject *parent = nullptr);
    ~ResourceManager() override = default;

    /**
     * @brief Attempts to allocate a runway to a flight under mutual exclusion.
     * Enforces the following invariants:
     *   1. Runway must not be currently in Maintenance.
     *   2. Runway must not already have an active allocation (no double allocation).
     *   3. Flight must not already hold another active runway (no multi-runway flight).
     *
     * @param runwayId  ID of the runway to allocate.
     * @param flightId  ID of the flight requesting the runway.
     * @param outError  Optional output string receiving descriptive failure reasons.
     * @return true if runway was successfully reserved; false otherwise.
     */
    bool allocateRunway(int runwayId, int flightId, QString *outError = nullptr);

    /**
     * @brief Releases an active runway allocation under mutual exclusion.
     * Validates that the runway currently holds an active allocation before releasing.
     *
     * @param runwayId  ID of the runway to release.
     * @param outError  Optional output string receiving descriptive failure reasons.
     * @return true if runway was active and successfully released; false otherwise.
     */
    bool releaseRunway(int runwayId, QString *outError = nullptr);

    /**
     * @brief Reverts an in-memory runway reservation if a subsequent persistence step fails.
     * Ensures failed allocations do not corrupt in-memory resource state.
     */
    void rollbackAllocation(int runwayId, int flightId);

    /**
     * @brief Confirms an in-memory allocation and updates critical section status.
     */
    void confirmAllocation(int runwayId, int flightId);

    /**
     * @brief Returns the current status of a runway (queries MySQL if connected, falls back to in-memory).
     */
    QString runwayStatus(int runwayId) const;

    /**
     * @brief Checks if a runway is currently reserved in memory.
     */
    bool isRunwayReserved(int runwayId) const;

    /**
     * @brief Returns the ID of the flight currently allocated to the given runway (0 if none).
     */
    int activeFlightForRunway(int runwayId) const;

    /**
     * @brief Returns the ID of the runway currently held by the given flight (0 if none).
     */
    int activeRunwayForFlight(int flightId) const;

    /**
     * @brief Sets or clears the in-memory maintenance status of a runway.
     */
    void setRunwayMaintenance(int runwayId, bool inMaintenance);

    /**
     * @brief Checks if a runway is marked as in maintenance.
     */
    bool isRunwayInMaintenance(int runwayId) const;

    /**
     * @brief Returns the current critical section lifecycle status for a runway.
     */
    QString criticalSectionStatus(int runwayId) const;

    /**
     * @brief Updates the critical section lifecycle status for a runway.
     */
    void setCriticalSectionStatus(int runwayId, const QString &status);

    /**
     * @brief Attempts to enter the critical section for a runway without blocking.
     */
    bool tryEnterCriticalSection(int runwayId);

    /**
     * @brief Exits the critical section for a runway and emits criticalSectionExited.
     */
    void exitCriticalSection(int runwayId, const QString &finalStatus = QStringLiteral("Critical Section Released"));

    /**
     * @brief Synchronizes in-memory allocations with active assignments in the database.
     */
    void syncFromDatabase();

    /**
     * @brief Resets in-memory allocation and maintenance state (primarily for tests).
     */
    void clearState();

    /**
     * @brief Returns a reference to the internal mutex (used by ScopedResourceLock and tests).
     */
    std::mutex &resourceMutex() const { return m_mutex; }

    /**
     * @brief Diagnostic metric: peak concurrent critical-section entries observed (must always be <= 1).
     */
    int maxConcurrentObserved() const { return m_maxConcurrentObserved; }

    // ── Unlocked internal helpers (used when caller already holds m_mutex) ────
    bool isRunwayReservedUnlocked(int runwayId) const;
    int activeFlightForRunwayUnlocked(int runwayId) const;
    int activeRunwayForFlightUnlocked(int flightId) const;
    bool isRunwayInMaintenanceUnlocked(int runwayId) const;
    void setRunwayMaintenanceUnlocked(int runwayId, bool inMaintenance);
    QString criticalSectionStatusUnlocked(int runwayId) const;
    void setCriticalSectionStatusUnlocked(int runwayId, const QString &status);
    void rollbackAllocationUnlocked(int runwayId, int flightId);
    void confirmAllocationUnlocked(int runwayId, int flightId);
    bool releaseRunwayUnlocked(int runwayId, QString *outError = nullptr);

signals:
    /**
     * @brief Emitted when a runway's operational status changes.
     */
    void runwayStatusChanged(int runwayId, const QString &newStatus);

    /**
     * @brief Emitted if allocation fails (runway occupied, maintenance, or invalid).
     */
    void allocationFailed(int runwayId, const QString &reason);

    /**
     * @brief Emitted when an operation enters the runway critical section.
     */
    void criticalSectionEntered(int runwayId, int flightId);

    /**
     * @brief Emitted when an operation exits the runway critical section.
     */
    void criticalSectionExited(int runwayId, const QString &finalStatus);

    /**
     * @brief Emitted when a runway is successfully reserved for a flight.
     */
    void runwayReserved(int runwayId, int flightId);

    /**
     * @brief Emitted when a runway is released and returned to available state.
     */
    void runwayReleased(int runwayId);

    /**
     * @brief Emitted when allocation is rejected due to concurrent contention or prior occupancy.
     */
    void allocationContentionRejected(int runwayId, int flightId, const QString &reason);

private:
    mutable std::mutex m_mutex;

    // In-memory state protected by m_mutex
    QMap<int, int>     m_activeAllocations;     // runwayId -> flightId
    QSet<int>          m_maintenanceRunways;    // runwayIds in maintenance
    QMap<int, QString> m_csStatus;              // runwayId -> Critical Section Status String

    // Concurrency tracking for verification
    int m_concurrentCount = 0;
    int m_maxConcurrentObserved = 0;
};
