#pragma once

#include <QString>

namespace Models {

/**
 * @brief Represents a runway in the ATC system.
 *
 * OS Concept Mapping (Phase 2):
 *   A Runway is a SHARED RESOURCE — analogous to a shared OS resource
 *   (e.g., a printer, a critical section).
 *   Mutual exclusion will be enforced by RunwayResourceManager (Day 19).
 *   Only one flight may occupy a runway at a time.
 *
 * DBMS Mapping:
 *   Persisted in the `runways` table (schema defined Day 3).
 *   Joined to `runway_assignments` for allocation history.
 */
struct Runway {
    int     id          = 0;
    QString name;               // e.g. "RWY-09L"
    int     lengthMeters = 0;
    // Status: Available | Occupied | Maintenance
    QString status      = "Available";

    Runway() = default;
};

} // namespace Models
