#include "VkChromeCookies.h"
#include "VkCrypto.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace VkChromeCookies {

QString defaultCefDir()
{
    const QString local = qEnvironmentVariable("LOCALAPPDATA");
    if (local.isEmpty())
        return {};
    return QDir(local).filePath(QStringLiteral("GameCenter/Cache/Chrome"));
}

// Ключ os_crypt из LocalPrefs.json (или Local State): base64 -> убрать префикс
// "DPAPI" (5 байт) -> DPAPI-расшифровать -> 32-байтный AES-ключ.
static QByteArray readMasterKey(const QString &cefDir, QString *err)
{
    const QStringList candidates = {QDir(cefDir).filePath(QStringLiteral("LocalPrefs.json")),
                                    QDir(cefDir).filePath(QStringLiteral("Local State"))};
    for (const QString &path : candidates) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        const QString enc = root.value(QStringLiteral("os_crypt"))
                                .toObject()
                                .value(QStringLiteral("encrypted_key"))
                                .toString();
        if (enc.isEmpty())
            continue;
        QByteArray blob = QByteArray::fromBase64(enc.toLatin1());
        if (blob.size() > 5 && blob.left(5) == QByteArrayLiteral("DPAPI"))
            blob = blob.mid(5);
        const QByteArray key = VkCrypto::dpapiUnprotect(blob);
        if (key.size() == 32)
            return key;
    }
    if (err)
        *err = QStringLiteral("не найден ключ os_crypt (LocalPrefs.json)");
    return {};
}

// encrypted_value: "v10" + nonce(12) + cipher + tag(16); иначе — legacy DPAPI.
static QString decryptValue(const QByteArray &enc, const QByteArray &key)
{
    if (enc.startsWith("v10")) {
        if (enc.size() < 3 + 12 + 16)
            return {};
        const QByteArray nonce = enc.mid(3, 12);
        const QByteArray tag = enc.right(16);
        const QByteArray cipher = enc.mid(15, enc.size() - 15 - 16);
        return QString::fromUtf8(VkCrypto::aesGcmDecrypt(key, nonce, cipher, tag));
    }
    if (enc.startsWith("v20")) // app-bound (новый Chrome) — не поддерживаем
        return {};
    return QString::fromUtf8(VkCrypto::dpapiUnprotect(enc)); // старый формат
}

QList<Cookie> decrypt(const QString &cefDir, const QStringList &hostsLike, QString *err)
{
    QList<Cookie> out;
    const QByteArray key = readMasterKey(cefDir, err);
    if (key.isEmpty())
        return out;

    QString dbPath = QDir(cefDir).filePath(QStringLiteral("Network/Cookies"));
    if (!QFile::exists(dbPath))
        dbPath = QDir(cefDir).filePath(QStringLiteral("Cookies"));
    if (!QFile::exists(dbPath)) {
        if (err)
            *err = QStringLiteral("не найден файл Cookies");
        return out;
    }

    // Копируем БД во временный файл: оригинал может быть заблокирован.
    const QString tmp = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                            .filePath(QStringLiteral("rfvk_cookies_")
                                      + QUuid::createUuid().toString(QUuid::Id128) + QStringLiteral(".db"));
    if (!QFile::copy(dbPath, tmp)) {
        if (err)
            *err = QStringLiteral("не удалось прочитать БД cookie");
        return out;
    }

    {
        const QString conn = QStringLiteral("rfvk_cookies_") + QUuid::createUuid().toString(QUuid::Id128);
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(tmp);
        if (db.open()) {
            QSqlQuery q(QStringLiteral("SELECT host_key, name, encrypted_value FROM cookies"), db);
            while (q.next()) {
                const QString host = q.value(0).toString();
                bool match = false;
                for (const QString &h : hostsLike)
                    if (host.contains(h)) { match = true; break; }
                if (!match)
                    continue;
                const QString value = decryptValue(q.value(2).toByteArray(), key);
                if (!value.isEmpty())
                    out.append({host, q.value(1).toString(), value});
            }
            db.close();
        } else if (err) {
            *err = QStringLiteral("SQLite: ") + db.lastError().text();
        }
        QSqlDatabase::removeDatabase(conn);
    }
    QFile::remove(tmp);
    return out;
}

} // namespace VkChromeCookies
