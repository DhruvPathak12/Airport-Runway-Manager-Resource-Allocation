# Security Considerations and Risk Assessment — SmartATC

## 1. Security Architecture Overview

SmartATC is designed according to **Defense in Depth** principles. Security checks are enforced across multiple architectural boundaries:
1. **User Interface Layer:** Controls are conditionally hidden or disabled based on session roles.
2. **Application Service Layer:** Business logic methods enforce explicit authentication and role-based permissions (`Session::hasPermission()`) before executing any action.
3. **Database Layer:** Multi-statement transactions and parameterized queries prevent unauthorized data mutation and SQL injection.

---

## 2. Authentication & Credential Storage

### 2.1 Password Hashing Implementation
Passwords are cryptographically hashed using SHA-256 via Qt's `QCryptographicHash`:
```cpp
QString PasswordUtils::hash(const QString &plaintext)
{
    if (plaintext.isEmpty()) return QString();
    const QByteArray bytes = plaintext.toUtf8();
    const QByteArray digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}
```
- Passwords are never stored, transmitted, or logged in plaintext.
- The `users.password_hash` column is defined as `CHAR(64)` storing hexadecimal digests.

### 2.2 Security Limitation & Production Recommendations
> [!WARNING]
> **Cryptographic Evaluation:**
> The current implementation uses **unsalted SHA-256** for academic and simulation purposes. While significantly superior to plaintext, unsalted SHA-256 is vulnerable to precomputed dictionary attacks (rainbow tables) and high-speed GPU brute-force attacks.
> 
> **Production Hardening Recommendation:**
> Before deploying SmartATC in an enterprise or production environment, the hashing algorithm must be upgraded to **Argon2id** (RFC 9106) or **bcrypt** with a cryptographically secure random 16-byte salt generated per user and an adaptive work factor (iteration cost $\ge 12$).

---

## 3. Database Credential Management

- **External Configuration:** Database connection parameters (`host`, `port`, `database`, `username`, `password`) are parsed from an external file: `db.conf`.
- **Version Control Exclusion:** `db.conf` is explicitly registered in `.gitignore` and is **never committed to Git**. A clean template (`db.conf.example`) is provided for developers.
- **Sanitized Logging:** `DatabaseManager` logs database connection events, hostnames, and database names, but **strictly omits database passwords from logs and console outputs**.

---

## 4. SQL Injection Protection

All database interactions throughout `ApplicationService` and `DatabaseManager` use strongly typed parameterized queries with positional or named placeholders:
- **Rule:** Direct string concatenation of user-supplied variables into SQL statements is prohibited.
- **Example:**
  ```cpp
  QSqlQuery q(db);
  q.prepare("UPDATE flights SET operational_status = :st WHERE flight_id = :id;");
  q.bindValue(":st", newStatus);
  q.bindValue(":id", flightId);
  q.exec();
  ```
- **Benefit:** The database engine parses SQL syntax separately from data literals, completely preventing SQL injection attacks (OWASP Top 10 A03).

---

## 5. CLI Account Provisioning Security

The developer CLI tool (`SmartATC --provision-user`) provides administrative account setup while defending against common credential leakage vectors:

### 5.1 Defense Against Process Argument Snooping
- **No Command-Line Passwords:** The CLI command **never accepts passwords as command-line arguments** (e.g. `--password <secret>`).
- **Threat Mitigated:** On Windows and Linux, process command-line arguments are visible to any unprivileged user via `Get-Process`, `Task Manager`, or `ps aux`. By omitting password CLI flags, argument inspection is completely mitigated.

### 5.2 Defense Against Shell History Logging
- **Threat Mitigated:** Commands typed into PowerShell (`PSReadLine`) or Bash history files (`.bash_history`) persist in plaintext on disk.
- **Implementation:** The CLI reads passwords via `PasswordUtils::readPasswordSecurely()`, which prompts the user interactively and disables console echoing (`ENABLE_ECHO_INPUT` disabled via Win32 API). Passwords are never written to shell history files.

---

## 6. Role-Based Access Control (RBAC) Enforcement

The system strictly enforces authorization at the service layer:
```cpp
if (!m_session.hasPermission(QStringLiteral("action:DeclareEmergency"))) {
    *outError = QStringLiteral("Access denied: role '%1' is not authorized to declare emergency.")
                    .arg(m_session.role());
    return false;
}
```
- **Defense:** Even if an attacker attempts to call `ApplicationService` methods directly or modifies the client GUI binaries to un-hide disabled buttons, the service layer rejects unauthorized actions with structured error messages.

---

## 7. Audit Logging & Non-Repudiation

- All critical actions (`RUNWAY_ASSIGN`, `RUNWAY_RELEASE`, emergency declarations) insert an immutable audit record into `operation_logs`.
- Records store the user ID, operation type, affected flight ID, affected runway ID, transaction status (`Committed`, `Rolled Back`), and an exact UTC timestamp.
- The table cannot be modified or truncated by non-admin users.
