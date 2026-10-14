#include "PasswordUtils.h"
#include <QStringList>

QString PasswordUtils::hash(const QString &plaintext)
{
    if (plaintext.isEmpty()) {
        return QString();
    }
    const QByteArray bytes = plaintext.toUtf8();
    const QByteArray digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}

bool PasswordUtils::verify(const QString &plaintext, const QString &storedHash)
{
    if (plaintext.isEmpty() || storedHash.isEmpty()) {
        return false;
    }
    const QString calculated = hash(plaintext);
    return calculated.compare(storedHash, Qt::CaseInsensitive) == 0;
}

#if defined(Q_OS_WIN)
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif
#include <iostream>

QString PasswordUtils::readPasswordSecurely(const QString &prompt)
{
    if (!prompt.isEmpty()) {
        std::cout << prompt.toStdString();
        std::cout.flush();
    }

    std::string password;

#if defined(Q_OS_WIN)
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    bool isConsole = GetConsoleMode(hStdin, &mode);
    if (isConsole) {
        // Disable ECHO
        SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
    }
    std::getline(std::cin, password);
    if (isConsole) {
        // Restore console mode
        SetConsoleMode(hStdin, mode);
        std::cout << std::endl;
    }
#else
    termios oldt;
    bool isTty = isatty(STDIN_FILENO);
    if (isTty) {
        tcgetattr(STDIN_FILENO, &oldt);
        termios newt = oldt;
        newt.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    }
    std::getline(std::cin, password);
    if (isTty) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        std::cout << std::endl;
    }
#endif

    // Trim trailing carriage return if any (e.g., from Windows line endings)
    while (!password.empty() && (password.back() == '\r' || password.back() == '\n')) {
        password.pop_back();
    }

    return QString::fromStdString(password);
}

bool PasswordUtils::validateAccountInput(const QString &username, const QString &roleName,
                                         const QString &fullName, const QString &password,
                                         QString *outError)
{
    if (username.trimmed().isEmpty()) {
        if (outError) *outError = QStringLiteral("Username cannot be empty.");
        return false;
    }

    if (username.length() > 50) {
        if (outError) *outError = QStringLiteral("Username cannot exceed 50 characters.");
        return false;
    }

    if (roleName.trimmed().isEmpty()) {
        if (outError) *outError = QStringLiteral("Role cannot be empty.");
        return false;
    }

    // Role must match one of the three established system roles
    static const QStringList validRoles = {
        QStringLiteral("Admin"),
        QStringLiteral("ATC"),
        QStringLiteral("Airport Operator")
    };

    bool validRole = false;
    for (const QString &r : validRoles) {
        if (roleName.compare(r, Qt::CaseInsensitive) == 0) {
            validRole = true;
            break;
        }
    }

    if (!validRole) {
        if (outError) *outError = QStringLiteral("Role '%1' is invalid. Valid roles are: Admin, ATC, Airport Operator.")
                                    .arg(roleName);
        return false;
    }

    if (fullName.length() > 100) {
        if (outError) *outError = QStringLiteral("Full name cannot exceed 100 characters.");
        return false;
    }

    if (password.isEmpty()) {
        if (outError) *outError = QStringLiteral("Password cannot be empty.");
        return false;
    }

    return true;
}
