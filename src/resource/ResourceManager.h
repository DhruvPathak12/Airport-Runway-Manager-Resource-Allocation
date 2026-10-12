#pragma once

#include <QObject>
#include "models/Runway.h"

/**
 * @brief Foundation for Runway Resource Management.
 *
 * ── OS Concept Mapping ────────────────────────────────────────────────────────
 *   A Runway is a shared OS resource. This class enforces mutual exclusion
 *   to prevent concurrent allocation conflicts.
 *
 *   Runway      → Shared OS resource (like a printer / critical section)
 *   allocate()  → Enter critical section (resource acquisition)
 *   release()   → Exit critical section (resource release)
 *
 *   Phase 2 (Day 19–20):
 *     One std::mutex per runway ID prevents race conditions.
 *     std::timed_mutex used with timeout to prevent deadlock.
 *     Lock ordering strictly enforced to avoid circular waits.
 *
 * ── DBMS Concept Mapping ──────────────────────────────────────────────────────
 *   Phase 2 (Day 21):
 *     Runway allocation is wrapped in an ACID transaction:
 *       BEGIN TRANSACTION
 *         SELECT ... FOR UPDATE   (row-level lock — isolation)
 *         UPDATE runways SET status='Occupied' WHERE status='Available'
 *         INSERT INTO runway_assignments ...
 *         UPDATE flights SET operational_status ...
 *         INSERT INTO operation_logs ...
 *       COMMIT / ROLLBACK
 *
 * ── Phase scope ───────────────────────────────────────────────────────────────
 *   Day 2  : Interface only. No allocation. No mutex. No transactions.
 *   Day 14 : Basic runway management implemented.
 *   Day 19 : Mutex / critical section added.
 *   Day 21 : ACID transaction wraps allocation.
 */
class ResourceManager : public QObject
{
    Q_OBJECT

public:
    explicit ResourceManager(QObject *parent = nullptr);
    ~ResourceManager() override = default;

    /**
     * @brief Attempts to allocate a runway to a flight.
     * @param runwayId  ID of the runway to allocate.
     * @param flightId  ID of the flight requesting the runway.
     * @return true if allocation succeeded.
     *
     * Implementation: Day 14 (basic), Day 19 (with mutex), Day 21 (with transaction).
     */
    bool allocateRunway(int runwayId, int flightId);

    /**
     * @brief Releases a runway after a flight completes.
     * @param runwayId  ID of the runway to release.
     *
     * Implementation: Day 14.
     */
    bool releaseRunway(int runwayId);

    /**
     * @brief Returns the current status of a runway.
     * Implementation: Day 14.
     */
    QString runwayStatus(int runwayId) const;

signals:
    /**
     * @brief Emitted when a runway's status changes.
     * Connected to the Runway Management UI widget.
     */
    void runwayStatusChanged(int runwayId, const QString &newStatus);

    /**
     * @brief Emitted if allocation fails (e.g., runway occupied or transaction rollback).
     */
    void allocationFailed(int runwayId, const QString &reason);
};
