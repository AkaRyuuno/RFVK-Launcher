#include "AccountFile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>

namespace AccountFile {
namespace {
const QString format = QStringLiteral("rfvk-accounts");
constexpr int version = 2;
}

QString read(const QString &path, QList<Entry> &entries)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("Не удалось открыть файл аккаунтов");
    const QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError)
        return QStringLiteral("Не удалось прочитать файл аккаунтов");

    const QJsonObject root = QJsonDocument::fromJson(data).object();
    const QJsonValue records = root.value(QStringLiteral("accounts"));
    if (root.value(QStringLiteral("format")).toString() != format
        || root.value(QStringLiteral("version")).toInt() != version || !records.isArray())
        return QStringLiteral("Неподдерживаемый формат файла аккаунтов RFVK Launcher");

    QList<Entry> parsed;
    const QJsonArray accounts = records.toArray();
    for (const QJsonValue &value : accounts) {
        const QJsonObject object = value.toObject();
        Entry entry;
        Account &account = entry.profile;
        account.login = object.value(QStringLiteral("login")).toString().trimmed();
        if (account.login.isEmpty())
            return QStringLiteral("В файле аккаунтов есть запись без логина");
        account.id = account.login;
        account.loginKind = object.value(QStringLiteral("loginKind")).toString();
        account.nick = object.value(QStringLiteral("nick")).toString();
        if (account.nick.isEmpty())
            account.nick = account.login;
        account.uid = object.value(QStringLiteral("uid")).toString();
        account.persId = object.value(QStringLiteral("persId")).toString();

        const QJsonValue session = object.value(QStringLiteral("session"));
        if (!session.isUndefined()) {
            const QJsonObject fields = session.toObject();
            entry.refreshToken = fields.value(QStringLiteral("refreshToken")).toString();
            entry.refreshExpires = fields.value(QStringLiteral("refreshExpires")).toInteger(-1);
            if (entry.refreshToken.trimmed().isEmpty()
                || entry.refreshToken != QString::fromLatin1(entry.refreshToken.toLatin1())
                || entry.refreshToken.contains(QChar::Null) || entry.refreshExpires < 0)
                return QStringLiteral("В файле аккаунтов есть некорректная сессия");
        }
        parsed.append(std::move(entry));
    }
    entries = std::move(parsed);
    return {};
}

QString write(const QString &path, const QList<Entry> &entries)
{
    if (entries.isEmpty())
        return QStringLiteral("Нет аккаунтов для экспорта");

    QJsonArray records;
    for (const Entry &entry : entries) {
        const Account &account = entry.profile;
        QJsonObject object{
            {QStringLiteral("login"), account.login},
            {QStringLiteral("loginKind"), account.loginKind},
            {QStringLiteral("nick"), account.nick},
            {QStringLiteral("uid"), account.uid},
            {QStringLiteral("persId"), account.persId}
        };
        if (!entry.refreshToken.isEmpty()) {
            object.insert(QStringLiteral("session"), QJsonObject{
                {QStringLiteral("refreshToken"), entry.refreshToken},
                {QStringLiteral("refreshExpires"), entry.refreshExpires}
            });
        }
        records.append(object);
    }

    const QJsonDocument document(QJsonObject{
        {QStringLiteral("format"), format},
        {QStringLiteral("version"), version},
        {QStringLiteral("accounts"), records}
    });
    const QByteArray data = document.toJson();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        return QStringLiteral("Не удалось сохранить файл аккаунтов");
    return {};
}

int findProfile(const QList<Account> &accounts, const Account &profile)
{
    int loginMatch = -1;
    for (qsizetype row = 0; row < accounts.size(); ++row) {
        const Account &account = accounts.at(row);
        if (!profile.uid.isEmpty() && account.uid == profile.uid)
            return row;
        if (loginMatch < 0 && account.login.compare(profile.login, Qt::CaseInsensitive) == 0)
            loginMatch = row;
    }
    return loginMatch;
}

QString ImportResult::summary() const
{
    if (added == 0 && sessions == 0 && skipped == 0)
        return QStringLiteral("В файле нет аккаунтов");
    QString message = QStringLiteral("Добавлено аккаунтов: %1 · Импортировано сессий: %2").arg(added).arg(sessions);
    if (skipped > 0)
        message += QStringLiteral(" · Пропущено: %1").arg(skipped);
    return message;
}

QString exportSummary(const QList<Entry> &entries)
{
    const auto sessions = std::count_if(entries.cbegin(), entries.cend(), [](const Entry &entry) {
        return !entry.refreshToken.isEmpty();
    });
    return QStringLiteral("Экспортировано аккаунтов: %1 · Сессий: %2").arg(entries.size()).arg(sessions);
}

} // namespace AccountFile
