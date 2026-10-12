#include "PasswordUtils.h"

QString PasswordUtils::hash(const QString &plaintext)
{
    // TODO (Day 6): Implement.
    // Implementation:
    //   QByteArray bytes = plaintext.toUtf8();
    //   QByteArray hashed = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    //   return QString(hashed.toHex());
    Q_UNUSED(plaintext)
    return QString();
}

bool PasswordUtils::verify(const QString &plaintext, const QString &storedHash)
{
    // TODO (Day 6): Implement.
    // return hash(plaintext) == storedHash;
    Q_UNUSED(plaintext)
    Q_UNUSED(storedHash)
    return false;
}
