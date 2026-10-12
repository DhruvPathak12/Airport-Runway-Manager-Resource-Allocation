#pragma once

#include <QString>
#include <QCryptographicHash>

/**
 * @brief Password utility functions using Qt 6 built-in SHA-256.
 *
 * SECURITY RULES (FINAL — do not change):
 *   - Use QCryptographicHash::Sha256 ONLY.
 *   - Do NOT add OpenSSL as a dependency.
 *   - Never store or log the plaintext password anywhere.
 *   - Always store only the hex hash in the database.
 *
 * Implementation: Day 6 (Login + roles).
 */
class PasswordUtils
{
public:
    /**
     * @brief Hashes a plaintext password using SHA-256.
     * @param plaintext  The user's plaintext password (never stored).
     * @return  Lowercase hex SHA-256 string (64 characters).
     *
     * Implementation: Day 6.
     */
    static QString hash(const QString &plaintext);

    /**
     * @brief Verifies a plaintext password against a stored hash.
     * @param plaintext  The candidate plaintext password.
     * @param storedHash The SHA-256 hex hash retrieved from the database.
     * @return true if hash(plaintext) == storedHash.
     *
     * Implementation: Day 6.
     */
    static bool verify(const QString &plaintext, const QString &storedHash);

private:
    PasswordUtils() = delete;  // Static utility — not instantiable
};
