#pragma once

#include <QString>
#include <QStringList>

/**
 * @brief Represents an in-memory authenticated user session.
 *
 * Security rules:
 * - NEVER persists or stores plaintext passwords.
 * - Created ONLY upon successful database authentication or valid test injection.
 * - Cleared completely upon logout.
 * - In-memory only; never written to disk or configuration files.
 */
class Session
{
public:
    Session() = default;
    Session(int userId, const QString &username, const QString &fullName, const QString &role);

    bool isValid() const { return m_userId > 0 && !m_username.isEmpty(); }
    void clear();

    int userId() const { return m_userId; }
    QString username() const { return m_username; }
    QString fullName() const { return m_fullName; }
    QString role() const { return m_role; }

    // Role helpers
    bool isAdmin() const { return m_role.compare(QStringLiteral("Admin"), Qt::CaseInsensitive) == 0; }
    bool isATC() const { return m_role.compare(QStringLiteral("ATC"), Qt::CaseInsensitive) == 0; }
    bool isOperator() const { return m_role.compare(QStringLiteral("Airport Operator"), Qt::CaseInsensitive) == 0; }

    /**
     * @brief Centralized authorization check for UI pages and actions.
     * @param permission  The permission or resource key to check.
     * @return true if the session's role is granted access.
     */
    bool hasPermission(const QString &permission) const;

    /**
     * @brief Returns a list of navigation labels allowed for this session's role.
     */
    QStringList allowedNavItems() const;

private:
    int     m_userId   = 0;
    QString m_username;
    QString m_fullName;
    QString m_role;
};
