#pragma once

#include <QString>
#include <QDateTime>
#include <QStringList>

#include "models/Flight.h"

namespace Models {

/**
 * @brief Represents the lifecycle execution states of a simulated process.
 *
 * Conceptual States:
 *   - New: Conceptual state for an eligible flight prior to ready-queue entry.
 *   - Ready: Entered the scheduling queue; awaiting CPU/runway assignment (DB: 'Ready').
 *   - Running: Currently executing / scheduled (DB: 'Running').
 *   - Blocked: Waiting on external resource / runway availability (DB: 'Blocked').
 *   - Completed: Processing finished; metrics calculated (DB: 'Completed').
 *
 * DBMS Mapping:
 *   Mapped to `scheduling_queue.process_state` ENUM('Ready','Running','Completed','Blocked').
 *   'New' maps to 'Ready' upon entry into the persistent scheduling queue.
 */
enum class ProcessState {
    New,
    Ready,
    Running,
    Blocked,
    Completed
};

/**
 * @brief Process Control Block (PCB) representing a simulated OS process for a flight.
 *
 * ── OS Concept Mapping ────────────────────────────────────────────────────────
 *   In this simulation, an air traffic flight operation represents an OS process.
 *   - Flight ID / PID: Primary unique process identifier linking flight to PCB.
 *   - Queue ID: Persistent scheduling queue entry identifier.
 *   - Base Priority: Static process priority (1=Highest … 5=Lowest).
 *   - Effective Priority: Dynamically adjusted priority (separated to support Phase 2 aging).
 *   - Service Time: CPU Burst Time (simulated runway service duration in seconds, default 60s).
 *   - Arrival Time: Timestamp when the process entered the scheduling queue.
 *   - Process State: Simulated process lifecycle state (Ready, Running, Blocked, Completed).
 *   - Metrics: Waiting Time (WT), Turnaround Time (TAT), Completion Time (CT).
 *     These remain uncalculated (-1 / invalid) until Day 11/12 scheduler execution.
 *
 * ── DBMS Mapping ─────────────────────────────────────────────────────────────
 *   Persisted in the existing `scheduling_queue` table (8-table schema frozen on Day 3).
 *   Foreign key `flight_id` links to `flights.flight_id` with 1:0..1 uniqueness.
 *
 *   NOTE: These are simulated processes represented by C++ objects for OS/DBMS
 *   demonstration purposes; they are not kernel-level OS processes.
 */
struct Process {
    // ── Primary identity ─────────────────────────────────────────────────────
    int             id                 = 0;  // maps to scheduling_queue.queue_id (0 = unsaved)
    int             queueId            = 0;  // explicit alias for id
    int             flightId           = 0;  // maps to scheduling_queue.flight_id (simulated PID)

    // ── Scheduling attributes ────────────────────────────────────────────────
    QDateTime       arrivalTime;             // timestamp entered ready queue
    int             basePriority       = 3;  // static priority (1=highest … 5=lowest)
    int             effectivePriority  = 3;  // dynamic priority (equals basePriority initially)
    int             serviceTime        = 60; // CPU burst time in seconds (default 60s)

    // ── Performance metrics (uncalculated until scheduler runs Day 11/12) ────
    int             waitingTime        = -1; // seconds in ready queue (-1 = uncalculated/NULL)
    int             turnaroundTime     = -1; // completion - arrival (-1 = uncalculated/NULL)
    QDateTime       completionTime;          // timestamp completed (invalid = NULL)

    // ── Process State ────────────────────────────────────────────────────────
    ProcessState    state              = ProcessState::Ready;

    // ── Presentation metadata (joined from flights table) ────────────────────
    QString         flightNumber;
    QString         airline;
    QString         operationalStatus;

    // ── Constructors ─────────────────────────────────────────────────────────
    Process();
    Process(int qid, int fid, int priority = 3, int burstSec = 60, ProcessState st = ProcessState::Ready);

    // ── Identity & State Helpers ─────────────────────────────────────────────
    bool isPersisted() const { return id > 0; }
    bool isNew() const { return id == 0; }
    int  processId() const { return flightId; }
    bool isValid(QString *outError = nullptr) const { return validate(outError); }

    int          getQueueId() const { return queueId; }
    int          getFlightId() const { return flightId; }
    int          priority() const { return basePriority; }
    ProcessState processState() const { return state; }
    QString      processStateString() const { return stateToString(state); }

    void setQueueId(int qid) {
        id = qid;
        queueId = qid;
    }

    int  burstTime() const { return serviceTime; }
    void setBurstTime(int sec) { serviceTime = sec; }

    bool hasWaitingTime() const { return waitingTime >= 0; }
    bool hasTurnaroundTime() const { return turnaroundTime >= 0; }
    bool hasCompletionTime() const { return completionTime.isValid(); }

    // ── Metric Calculation (Day 12) ──────────────────────────────────────────
    /**
     * @brief Computes Completion Time, Turnaround Time, and Waiting Time for a completed process.
     * Formulas:
     *   - CT = startTime + serviceTime (or provided completion timestamp)
     *   - TAT = CT - AT (in seconds)
     *   - WT = TAT - BT (in seconds, equivalent to startTime - AT)
     *
     * @param completedAt Exact completion timestamp. Must be >= arrivalTime.
     * @param outError    Optional output string receiving failure explanation.
     * @return true if metrics were computed and validated successfully.
     */
    bool calculateMetrics(const QDateTime &completedAt, QString *outError = nullptr);

    /**
     * @brief Computes Completion Time, Turnaround Time, and Waiting Time given execution start timestamp.
     *
     * @param startedAt Execution start timestamp. Must be >= arrivalTime.
     * @param outError  Optional output string receiving failure explanation.
     * @return true if metrics were computed and validated successfully.
     */
    bool calculateMetricsFromStart(const QDateTime &startedAt, QString *outError = nullptr);

    // ── Validation ───────────────────────────────────────────────────────────
    /**
     * @brief Validates PCB fields against schema constraints and OS simulation rules.
     * @param outError Optional output string receiving validation failure explanation.
     * @return true if all fields are valid.
     */
    bool validate(QString *outError = nullptr) const;

    // ── Validated State Transitions (Day 10) ──────────────────────────────────
    /**
     * @brief Checks whether transitioning to the target state is allowed by process lifecycle.
     * Supported transitions:
     *   - New     -> Ready
     *   - Ready   -> Running
     *   - Running -> Completed
     *   - Running -> Blocked
     *   - Blocked -> Ready
     *
     * @param targetState Proposed destination state.
     * @param outReason   Optional output string explaining why transition is invalid if rejected.
     * @return true if the transition is allowed.
     */
    bool canTransitionTo(ProcessState targetState, QString *outReason = nullptr) const;

    /**
     * @brief Explicitly transitions the process to targetState if allowed.
     * Leaves current state unmodified if the transition is rejected.
     *
     * @param targetState Proposed destination state.
     * @param outError    Optional output string receiving error message on rejection.
     * @return true if state was updated.
     */
    bool transitionTo(ProcessState targetState, QString *outError = nullptr);

    // ── String Conversions ───────────────────────────────────────────────────
    static QString      stateToString(ProcessState st);
    static QString      stateToDbString(ProcessState st);
    static ProcessState stateFromString(const QString &str, bool *ok = nullptr);
    static ProcessState stateFromDbString(const QString &str, bool *ok = nullptr) { return stateFromString(str, ok); }

    // ── Flight-to-Process Mapping ────────────────────────────────────────────
    /**
     * @brief Determines whether a flight is eligible to enter the scheduling queue.
     * Excludes terminal and cancelled flights (Landed, Departed, Cancelled).
     */
    static bool isFlightEligible(const Flight &flight, QString *outReason = nullptr);

    /**
     * @brief Constructs a Process Control Block (PCB) from an eligible Flight.
     * Maps flight ID, base priority, initial effective priority, and user service time.
     * Leaves metrics uncalculated.
     */
    static Process fromFlight(const Flight &flight, int serviceTimeSeconds = 60);
};

// Aliases for OS design clarity
using ProcessControlBlock = Process;
using PCB = Process;

} // namespace Models
