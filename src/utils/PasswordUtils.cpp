#include "PasswordUtils.h"

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
