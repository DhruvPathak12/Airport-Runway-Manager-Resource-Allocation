#pragma once

#include <QList>
#include <QString>

#include "models/Process.h"

namespace SchedulerComponents {

/**
 * @brief In-memory ready queue holding simulated processes eligible for scheduling.
 *
 * ── OS Concept Mapping ────────────────────────────────────────────────────────
 *   The Ready Queue represents the pool of runnable processes awaiting CPU/runway execution.
 *   - Strictly maintains FIFO insertion order for Day 10 (no priority sorting).
 *   - Only processes in `ProcessState::Ready` may enter.
 *   - Duplicates are strictly rejected using the simulated PID (`flight_id`) and `queue_id`.
 *   - Explicit state transition support ensures processes moving out of `Ready` (e.g. to `Running`)
 *     are atomically removed to prevent stale queue entries.
 *
 * ── Concurrency & Persistence Boundaries ──────────────────────────────────────
 *   This is an in-memory single-threaded simulation queue.
 *   Synchronization mutexes belong to Days 19–20.
 *   Database persistence is handled at the service layer, not within this class.
 */
class ReadyQueue
{
public:
    ReadyQueue() = default;
    ~ReadyQueue() = default;

    // ── Enqueue & Dequeue (FIFO) ─────────────────────────────────────────────
    /**
     * @brief Adds an eligible ready process to the tail of the queue.
     * Rejects duplicate entries, invalid process data, and non-Ready processes.
     */
    bool enqueue(const Models::Process &process, QString *outError = nullptr);

    /**
     * @brief Dequeues the front process in FIFO order.
     * @param outProcess Optional output receiving the dequeued process.
     * @return true if dequeued; false if queue was empty.
     */
    bool dequeue(Models::Process *outProcess = nullptr, QString *outError = nullptr);

    // ── Removal ──────────────────────────────────────────────────────────────
    /**
     * @brief Removes a process identified by its simulated process ID (flightId).
     */
    bool removeByFlightId(int flightId, Models::Process *outRemoved = nullptr, QString *outError = nullptr);

    /**
     * @brief Removes a process identified by its persistent database queueId.
     */
    bool removeByQueueId(int queueId, Models::Process *outRemoved = nullptr, QString *outError = nullptr);

    // ── Lookups & Membership ─────────────────────────────────────────────────
    bool containsFlight(int flightId) const;
    bool containsQueueId(int queueId) const;

    const Models::Process* findByFlightId(int flightId) const;
    Models::Process*       findByFlightId(int flightId);
    bool                   findByFlightId(int flightId, Models::Process *outProcess) const {
        const Models::Process *p = findByFlightId(flightId);
        if (p) {
            if (outProcess) *outProcess = *p;
            return true;
        }
        return false;
    }

    const Models::Process* findByQueueId(int queueId) const;
    Models::Process*       findByQueueId(int queueId);
    bool                   findByQueueId(int queueId, Models::Process *outProcess) const {
        const Models::Process *p = findByQueueId(queueId);
        if (p) {
            if (outProcess) *outProcess = *p;
            return true;
        }
        return false;
    }

    const Models::Process* peek() const;

    // ── Queue Inspection ─────────────────────────────────────────────────────
    int  size() const { return m_queue.size(); }
    bool isEmpty() const { return m_queue.isEmpty(); }
    void clear() { m_queue.clear(); }
    QList<Models::Process> allProcesses() const { return m_queue; }
    QList<Models::Process> toProcessList() const { return m_queue; }

    // ── State Consistency ────────────────────────────────────────────────────
    /**
     * @brief Transitions the state of a queued process.
     * When transitioning out of `ProcessState::Ready` (e.g. to `Running`),
     * the process is atomically removed from the ready queue to ensure state consistency.
     */
    bool transitionProcessState(int flightId,
                                Models::ProcessState newState,
                                Models::Process *outProcess = nullptr,
                                QString *outError = nullptr);

private:
    QList<Models::Process> m_queue;
};

} // namespace SchedulerComponents
