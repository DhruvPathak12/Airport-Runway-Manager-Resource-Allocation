#pragma once

#include <QString>
#include <QDateTime>

namespace Models {

enum class AssignmentStatus {
    Active,
    Released,
    Cancelled,
    Unknown
};

enum class AllocationResult {
    Success,
    RunwayNotFound,
    RunwayUnavailable,
    FlightNotFound,
    FlightIneligible,
    ConflictExistingAssignment,
    DatabaseError,
    Unauthorized,
    InvalidInput
};

/**
 * @brief Returns a human-readable explanation for an AllocationResult enum.
 */
inline QString allocationResultToString(AllocationResult res) {
    switch (res) {
    case AllocationResult::Success:
        return QStringLiteral("Allocation succeeded.");
    case AllocationResult::RunwayNotFound:
        return QStringLiteral("Runway not found.");
    case AllocationResult::RunwayUnavailable:
        return QStringLiteral("Runway is unavailable (occupied or in maintenance).");
    case AllocationResult::FlightNotFound:
        return QStringLiteral("Flight not found.");
    case AllocationResult::FlightIneligible:
        return QStringLiteral("Flight is ineligible for runway allocation.");
    case AllocationResult::ConflictExistingAssignment:
        return QStringLiteral("Active runway assignment conflict detected.");
    case AllocationResult::DatabaseError:
        return QStringLiteral("Database operation failed.");
    case AllocationResult::Unauthorized:
        return QStringLiteral("Operation unauthorized: ATC role required.");
    case AllocationResult::InvalidInput:
        return QStringLiteral("Invalid input parameters.");
    default:
        return QStringLiteral("Unknown allocation result.");
    }
}

/**
 * @brief Represents a runway assignment record in the ATC system.
 *
 * OS Concept Mapping (Phase 2):
 *   A runway assignment represents physical resource ownership in the simulation.
 *   An active assignment indicates that the runway (shared resource) is currently
 *   held by a specific flight process.
 *
 * DBMS Mapping:
 *   Persisted in the `runway_assignments` table:
 *     - assignment_id INT AUTO_INCREMENT PRIMARY KEY
 *     - flight_id     INT NOT NULL (FK -> flights)
 *     - runway_id     INT NOT NULL (FK -> runways)
 *     - assigned_by   INT NULL (FK -> users)
 *     - assigned_at   DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
 *     - released_at   DATETIME NULL DEFAULT NULL
 *     - status        ENUM('Active','Released','Cancelled') NOT NULL DEFAULT 'Active'
 */
struct RunwayAssignment {
    int         assignmentId       = 0;
    int         flightId           = 0;
    int         runwayId           = 0;
    int         assignedBy         = 0;
    QDateTime   assignedAt;
    QDateTime   releasedAt;
    QString     status             = QStringLiteral("Active"); // "Active" | "Released" | "Cancelled"

    // Presentation / JOIN metadata
    QString     flightNumber;
    QString     airline;
    QString     runwayNumber;
    QString     assignedByUsername;

    RunwayAssignment() = default;

    RunwayAssignment(int id, int fid, int rid, int uid, const QString &st = QStringLiteral("Active"))
        : assignmentId(id)
        , flightId(fid)
        , runwayId(rid)
        , assignedBy(uid)
        , assignedAt(QDateTime::currentDateTime())
        , status(st.trimmed())
    {
    }

    bool isActive() const {
        return status.compare(QStringLiteral("Active"), Qt::CaseInsensitive) == 0;
    }

    bool isReleased() const {
        return status.compare(QStringLiteral("Released"), Qt::CaseInsensitive) == 0;
    }

    bool isCancelled() const {
        return status.compare(QStringLiteral("Cancelled"), Qt::CaseInsensitive) == 0;
    }

    AssignmentStatus statusEnum() const {
        if (isActive())    return AssignmentStatus::Active;
        if (isReleased())  return AssignmentStatus::Released;
        if (isCancelled()) return AssignmentStatus::Cancelled;
        return AssignmentStatus::Unknown;
    }

    static QString statusToString(AssignmentStatus s) {
        switch (s) {
        case AssignmentStatus::Active:    return QStringLiteral("Active");
        case AssignmentStatus::Released:  return QStringLiteral("Released");
        case AssignmentStatus::Cancelled: return QStringLiteral("Cancelled");
        default:                          return QStringLiteral("Unknown");
        }
    }

    static AssignmentStatus stringToStatus(const QString &str) {
        QString trimmed = str.trimmed();
        if (trimmed.compare(QStringLiteral("Active"), Qt::CaseInsensitive) == 0)    return AssignmentStatus::Active;
        if (trimmed.compare(QStringLiteral("Released"), Qt::CaseInsensitive) == 0)  return AssignmentStatus::Released;
        if (trimmed.compare(QStringLiteral("Cancelled"), Qt::CaseInsensitive) == 0) return AssignmentStatus::Cancelled;
        return AssignmentStatus::Unknown;
    }
};

} // namespace Models
