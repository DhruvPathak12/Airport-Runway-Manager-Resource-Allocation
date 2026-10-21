#pragma once

#include <QString>

namespace Models {

enum class RunwayStatus {
    Available,
    Occupied,
    Maintenance,
    Unknown
};

enum class RunwayType {
    Landing,
    Takeoff,
    Both,
    Unknown
};

/**
 * @brief Represents a runway in the ATC system.
 *
 * OS Concept Mapping (Phase 2):
 *   A Runway is a SHARED RESOURCE — analogous to a shared OS resource
 *   (e.g., a printer, a critical section).
 *   Mutual exclusion will be enforced by RunwayResourceManager (Day 19).
 *   Only one flight may occupy a runway at a time.
 *   Day 14 establishes the management and state foundation:
 *     - Available: Free resource, eligible for allocation.
 *     - Occupied: Resource currently held by an active flight process.
 *     - Maintenance: Resource offline / disabled for maintenance.
 *
 * DBMS Mapping:
 *   Persisted in the `runways` table (schema defined Day 3):
 *     - runway_id     INT AUTO_INCREMENT PRIMARY KEY
 *     - runway_number VARCHAR(10) NOT NULL UNIQUE
 *     - runway_type   ENUM('Landing','Takeoff','Both') NOT NULL DEFAULT 'Both'
 *     - status        ENUM('Available','Occupied','Maintenance') NOT NULL DEFAULT 'Available'
 */
struct Runway {
    int     id                  = 0;  // maps to runway_id
    QString runwayNumber;             // e.g. "09L", "27R" (max 10 chars, UNIQUE)
    QString name;                     // Backward-compatibility alias for runwayNumber
    QString runwayType          = QStringLiteral("Both"); // "Landing" | "Takeoff" | "Both"
    QString status              = QStringLiteral("Available"); // "Available" | "Occupied" | "Maintenance"
    int     lengthMeters        = 0;  // Preserved for backward compatibility

    // Presentation / JOIN metadata (from active assignment if any)
    int     activeFlightId      = 0;
    QString activeFlightNumber;

    Runway() = default;

    Runway(int runwayId,
           const QString &number,
           const QString &type = QStringLiteral("Both"),
           const QString &st = QStringLiteral("Available"))
        : id(runwayId)
        , runwayNumber(number.trimmed().toUpper())
        , name(number.trimmed().toUpper())
        , runwayType(type.trimmed())
        , status(st.trimmed())
    {
        normalize();
    }

    /**
     * @brief Normalizes and returns the effective runway designation.
     */
    QString effectiveNumber() const {
        return !runwayNumber.trimmed().isEmpty() ? runwayNumber.trimmed().toUpper()
                                                 : name.trimmed().toUpper();
    }

    void normalize() {
        runwayNumber = effectiveNumber();
        name = runwayNumber;
        runwayType = runwayType.trimmed();
        if (runwayType.isEmpty()) {
            runwayType = QStringLiteral("Both");
        }
        status = status.trimmed();
        if (status.isEmpty()) {
            status = QStringLiteral("Available");
        }
    }

    // Status helpers
    bool isAvailable() const {
        return status.compare(QStringLiteral("Available"), Qt::CaseInsensitive) == 0;
    }

    bool isOccupied() const {
        return status.compare(QStringLiteral("Occupied"), Qt::CaseInsensitive) == 0;
    }

    bool isMaintenance() const {
        return status.compare(QStringLiteral("Maintenance"), Qt::CaseInsensitive) == 0;
    }

    RunwayStatus statusEnum() const {
        if (isAvailable())   return RunwayStatus::Available;
        if (isOccupied())    return RunwayStatus::Occupied;
        if (isMaintenance()) return RunwayStatus::Maintenance;
        return RunwayStatus::Unknown;
    }

    static QString statusToString(RunwayStatus s) {
        switch (s) {
        case RunwayStatus::Available:   return QStringLiteral("Available");
        case RunwayStatus::Occupied:    return QStringLiteral("Occupied");
        case RunwayStatus::Maintenance: return QStringLiteral("Maintenance");
        default:                        return QStringLiteral("Unknown");
        }
    }

    static RunwayStatus stringToStatus(const QString &str) {
        QString trimmed = str.trimmed();
        if (trimmed.compare(QStringLiteral("Available"), Qt::CaseInsensitive) == 0)   return RunwayStatus::Available;
        if (trimmed.compare(QStringLiteral("Occupied"), Qt::CaseInsensitive) == 0)    return RunwayStatus::Occupied;
        if (trimmed.compare(QStringLiteral("Maintenance"), Qt::CaseInsensitive) == 0) return RunwayStatus::Maintenance;
        return RunwayStatus::Unknown;
    }

    static QString typeToString(RunwayType t) {
        switch (t) {
        case RunwayType::Landing: return QStringLiteral("Landing");
        case RunwayType::Takeoff: return QStringLiteral("Takeoff");
        case RunwayType::Both:    return QStringLiteral("Both");
        default:                  return QStringLiteral("Both");
        }
    }

    static RunwayType stringToType(const QString &str) {
        QString trimmed = str.trimmed();
        if (trimmed.compare(QStringLiteral("Landing"), Qt::CaseInsensitive) == 0) return RunwayType::Landing;
        if (trimmed.compare(QStringLiteral("Takeoff"), Qt::CaseInsensitive) == 0) return RunwayType::Takeoff;
        if (trimmed.compare(QStringLiteral("Both"), Qt::CaseInsensitive) == 0)    return RunwayType::Both;
        return RunwayType::Unknown;
    }

    /**
     * @brief Validates runway attributes against schema and business constraints.
     */
    bool validate(QString *outError = nullptr) const {
        QString num = effectiveNumber();
        if (num.isEmpty()) {
            if (outError) *outError = QStringLiteral("Runway number / designation is required.");
            return false;
        }
        if (num.length() > 10) {
            if (outError) *outError = QStringLiteral("Runway number cannot exceed 10 characters.");
            return false;
        }

        static const QStringList validTypes = {
            QStringLiteral("Landing"),
            QStringLiteral("Takeoff"),
            QStringLiteral("Both")
        };
        QString type = runwayType.trimmed();
        if (!type.isEmpty() && !validTypes.contains(type, Qt::CaseInsensitive)) {
            if (outError) {
                *outError = QStringLiteral("Invalid runway type: '%1'. Must be Landing, Takeoff, or Both.").arg(runwayType);
            }
            return false;
        }

        static const QStringList validStatuses = {
            QStringLiteral("Available"),
            QStringLiteral("Occupied"),
            QStringLiteral("Maintenance")
        };
        QString st = status.trimmed();
        if (!validStatuses.contains(st, Qt::CaseInsensitive)) {
            if (outError) {
                *outError = QStringLiteral("Invalid runway status: '%1'. Must be Available, Occupied, or Maintenance.").arg(status);
            }
            return false;
        }

        return true;
    }
};

} // namespace Models
