#pragma once

#include <QString>

namespace Models {

/**
 * @brief Represents a system user.
 *
 * Three roles:
 *   ATC              — scheduling, queue, emergency override
 *   Airport Operator — runway assignment, resource management
 *   Admin            — user management, system logs, full access
 *
 * DBMS Mapping:
 *   Persisted in the `users` table (schema defined Day 3).
 *   Password stored ONLY as SHA-256 hex hash (implemented Day 6).
 *   Role resolved via FK to `roles` table.
 */
struct User {
    int     id           = 0;
    QString username;
    QString passwordHash;    // SHA-256 hex string — NEVER store plaintext
    QString role;            // "ATC" | "Airport Operator" | "Admin"
    QString fullName;
    bool    isActive     = true;

    User() = default;
};

} // namespace Models
