#pragma once
#include "Account.h"
#include <QList>

namespace AccountFile {

// Запись для переноса аккаунта. Токен используется только внутри бэкенда.
struct Entry {
    Account profile;
    QString refreshToken;
    qint64 refreshExpires = 0;
};

struct ImportResult {
    int added = 0;
    int sessions = 0;
    int skipped = 0;

    QString summary() const;
};

QString read(const QString &path, QList<Entry> &entries);
QString write(const QString &path, const QList<Entry> &entries);
QString exportSummary(const QList<Entry> &entries);
int findProfile(const QList<Account> &accounts, const Account &profile);

} // namespace AccountFile
