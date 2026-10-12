#pragma once

#include <QString>
#include <QDateTime>

namespace Models {

/**
 * @brief Represents a flight in the ATC system.
 *
 * OS Concept Mapping (Phase 2):
 *   A Flight maps to a simulated OS Process.
 *   flight_id     → process ID
 *   priority      → scheduling priority (lower = higher priority)
 *   status        → process state (Waiting, Running, Completed, etc.)
 *   burst_time_sec → CPU burst / service time
 *
 * DBMS Mapping:
 *   Persisted in the `flights` table (schema defined Day 3).
 *   Joined to `scheduling_queue` via flight_id for scheduling data.
 */
struct Flight {
    // ── Primary identity ─────────────────────────────────────────────────────
    int         id            = 0;
    QString     flightNumber;       // e.g. "AI-202"
    QString     airline;
    QString     origin;
    QString     destination;

    // ── Scheduling / OS mapping ───────────────────────────────────────────────
    int         priority      = 3;  // 1=Highest … 5=Low  (0 reserved: Emergency)
    bool        isEmergency   = false;

    // ── Operational status ────────────────────────────────────────────────────
    // Possible values: Scheduled, Taxiing, Cleared, Landed, Departed, Cancelled
    QString     operationalStatus = "Scheduled";

    // ── Timing ────────────────────────────────────────────────────────────────
    QDateTime   scheduledTime;
    QDateTime   actualTime;

    // ── FK references (populated by service layer) ────────────────────────────
    int         aircraftId    = 0;
    int         assignedRunwayId = 0;   // 0 = unassigned

    Flight() = default;
};

} // namespace Models
