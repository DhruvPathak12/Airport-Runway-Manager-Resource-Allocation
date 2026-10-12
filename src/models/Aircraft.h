#pragma once

#include <QString>

namespace Models {

/**
 * @brief Represents an aircraft registered in the system.
 *
 * DBMS Mapping:
 *   Persisted in the `aircraft` table (schema defined Day 3).
 *   Referenced by `flights.aircraft_id` (FK).
 */
struct Aircraft {
    int     id              = 0;
    QString registrationNo;     // e.g. "VT-ANB"
    QString model;              // e.g. "Boeing 737-800"
    QString airline;
    int     capacity        = 0;
    // Status: Active | Maintenance | Retired
    QString status          = "Active";

    Aircraft() = default;
};

} // namespace Models
