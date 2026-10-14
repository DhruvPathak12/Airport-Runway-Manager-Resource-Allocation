#pragma once

#include <QString>
#include <QStringList>

namespace Models {

/**
 * @brief Represents an aircraft registered in the system.
 *
 * DBMS Mapping:
 *   Persisted in the `aircraft` table (schema defined Day 3).
 *   Referenced by `flights.aircraft_id` (FK with ON DELETE SET NULL).
 */
struct Aircraft {
    int     id                  = 0;  // maps to aircraft_id
    QString registrationNumber;       // e.g. "VT-AT1" (max 20 chars, UNIQUE)
    QString registrationNo;           // Backward-compatibility alias
    QString aircraftType;             // e.g. "Boeing 737-800" (max 50 chars)
    QString model;                    // Backward-compatibility alias
    int     capacity            = 180; // SMALLINT UNSIGNED, positive integer
    QString status              = QStringLiteral("Active"); // Active | Maintenance | Retired

    // Presentation metadata (computed via JOIN with flights)
    int     assignedFlightsCount = 0;

    Aircraft() = default;

    /**
     * @brief Normalizes and returns the effective registration number.
     */
    QString effectiveRegistration() const {
        return !registrationNumber.trimmed().isEmpty() ? registrationNumber.trimmed().toUpper()
                                                       : registrationNo.trimmed().toUpper();
    }

    /**
     * @brief Normalizes and returns the effective aircraft type.
     */
    QString effectiveType() const {
        return !aircraftType.trimmed().isEmpty() ? aircraftType.trimmed()
                                                 : model.trimmed();
    }

    void normalize() {
        registrationNumber = effectiveRegistration().toUpper();
        registrationNo = registrationNumber;
        aircraftType = effectiveType();
        model = aircraftType;
        status = status.trimmed();
    }

    /**
     * @brief Validates aircraft fields against schema and business constraints.
     */
    bool validate(QString *outError = nullptr) {
        normalize();
        return validateConst(outError);
    }

    bool validate(QString *outError = nullptr) const {
        return validateConst(outError);
    }

private:
    bool validateConst(QString *outError = nullptr) const {
        QString reg = effectiveRegistration();
        if (reg.isEmpty()) {
            if (outError) *outError = QStringLiteral("Registration number is required.");
            return false;
        }
        if (reg.length() > 20) {
            if (outError) *outError = QStringLiteral("Registration number cannot exceed 20 characters.");
            return false;
        }

        QString type = effectiveType();
        if (type.isEmpty()) {
            if (outError) *outError = QStringLiteral("Aircraft type / model is required.");
            return false;
        }
        if (type.length() > 50) {
            if (outError) *outError = QStringLiteral("Aircraft type cannot exceed 50 characters.");
            return false;
        }

        if (capacity <= 0) {
            if (outError) *outError = QStringLiteral("Capacity must be greater than zero.");
            return false;
        }
        if (capacity > 65535) {
            if (outError) *outError = QStringLiteral("Capacity exceeds maximum permitted limit (65535).");
            return false;
        }

        static const QStringList validStatuses = {
            QStringLiteral("Active"),
            QStringLiteral("Maintenance"),
            QStringLiteral("Retired")
        };
        if (!validStatuses.contains(status.trimmed())) {
            if (outError) *outError = QStringLiteral("Invalid operational status: '%1'. Must be Active, Maintenance, or Retired.").arg(status);
            return false;
        }

        return true;
    }
};

} // namespace Models
