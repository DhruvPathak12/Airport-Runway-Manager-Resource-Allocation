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
    int         id            = 0;  // maps to flight_id
    QString     flightNumber;       // e.g. "AI-202" (max 10 chars)
    QString     airline;            // e.g. "Air India" (max 100 chars)
    QString     source;             // DB column: source (e.g. "DEL")
    QString     origin;             // Backward-compatibility alias for source
    QString     destination;        // e.g. "BOM" (max 100 chars)
    QString     flightType    = QStringLiteral("Arrival"); // "Arrival" | "Departure"

    // ── Scheduling / OS mapping ───────────────────────────────────────────────
    int         priority      = 3;  // 1=Highest … 5=Lowest (enforced by schema CHECK)
    bool        isEmergency   = false;

    // ── Operational status ────────────────────────────────────────────────────
    // Allowed values: Scheduled, Taxiing, Cleared, Landed, Departed, Cancelled
    QString     operationalStatus = QStringLiteral("Scheduled");

    // ── Timing ────────────────────────────────────────────────────────────────
    QDateTime   scheduledTime;
    QDateTime   actualTime;
    QDateTime   createdAt;

    // ── FK references & presentation metadata ─────────────────────────────────
    int         aircraftId    = 0;  // 0 = unassigned (NULL in database)
    int         assignedRunwayId = 0;   // 0 = unassigned
    QString     aircraftRegistration; // e.g. "VT-AT1 (Boeing 737-800)" from JOIN

    Flight() = default;

    /**
     * @brief Normalizes source and origin fields.
     */
    QString effectiveSource() const {
        return !source.trimmed().isEmpty() ? source.trimmed() : origin.trimmed();
    }

    /**
     * @brief Validates all flight attributes against application and schema constraints.
     */
    bool validate(QString *outError = nullptr) const {
        QString fn = flightNumber.trimmed();
        if (fn.isEmpty()) {
            if (outError) *outError = QStringLiteral("Flight number is required.");
            return false;
        }
        if (fn.length() > 10) {
            if (outError) *outError = QStringLiteral("Flight number cannot exceed 10 characters.");
            return false;
        }

        QString air = airline.trimmed();
        if (air.isEmpty()) {
            if (outError) *outError = QStringLiteral("Airline name is required.");
            return false;
        }
        if (air.length() > 100) {
            if (outError) *outError = QStringLiteral("Airline name cannot exceed 100 characters.");
            return false;
        }

        QString src = effectiveSource();
        if (src.isEmpty()) {
            if (outError) *outError = QStringLiteral("Source airport is required.");
            return false;
        }
        if (src.length() > 100) {
            if (outError) *outError = QStringLiteral("Source airport cannot exceed 100 characters.");
            return false;
        }

        QString dst = destination.trimmed();
        if (dst.isEmpty()) {
            if (outError) *outError = QStringLiteral("Destination airport is required.");
            return false;
        }
        if (dst.length() > 100) {
            if (outError) *outError = QStringLiteral("Destination airport cannot exceed 100 characters.");
            return false;
        }

        if (src.compare(dst, Qt::CaseInsensitive) == 0) {
            if (outError) *outError = QStringLiteral("Source and destination airports cannot be identical.");
            return false;
        }

        QString ft = flightType.trimmed();
        if (ft != QStringLiteral("Arrival") && ft != QStringLiteral("Departure")) {
            if (outError) *outError = QStringLiteral("Flight type must be 'Arrival' or 'Departure'.");
            return false;
        }

        if (priority < 1 || priority > 5) {
            if (outError) *outError = QStringLiteral("Priority must be between 1 (highest) and 5 (lowest).");
            return false;
        }

        static const QStringList validStatuses = {
            QStringLiteral("Scheduled"),
            QStringLiteral("Taxiing"),
            QStringLiteral("Cleared"),
            QStringLiteral("Landed"),
            QStringLiteral("Departed"),
            QStringLiteral("Cancelled")
        };
        if (!validStatuses.contains(operationalStatus.trimmed())) {
            if (outError) *outError = QStringLiteral("Invalid operational status: '%1'.").arg(operationalStatus);
            return false;
        }

        if (!scheduledTime.isValid()) {
            if (outError) *outError = QStringLiteral("Scheduled time must be a valid date and time.");
            return false;
        }

        if (aircraftId < 0) {
            if (outError) *outError = QStringLiteral("Aircraft ID cannot be negative.");
            return false;
        }

        return true;
    }
};

} // namespace Models
