#pragma once
// Источник прокси: список по URL (одна запись на строку), например
// https://cdn.jsdelivr.net/gh/proxifly/free-proxy-list@main/proxies/protocols/socks5/data.txt
//
// Строки: «socks5://host:port», «http://user:pass@host:port», «host:port»,
// «host:port:user:pass». Схема задаёт протокол (http/https — HTTP CONNECT,
// socks5/socks5h — SOCKS5); у строк без схемы — протокол источника.
// SOCKS4 и прочее rfvk_proxy.dll не умеет — такие строки пропускаются.

#include "ProxySlot.h"

#include <QByteArray>
#include <QString>
#include <QVector>

struct SourceEntry {
    enum State : quint8 { Unchecked, Ok, Bad };

    ProxySlot::Type type = ProxySlot::Type::Socks5;
    QString host;
    quint16 port = 0;
    QString username;
    QString password;
    State state = Unchecked;
    int pingMs = -1;
    qint64 checkedAt = 0; // время проверки, секунды Unix

    // Одна и та же точка выхода в разных загрузках списка.
    QString key() const { return QStringLiteral("%1|%2:%3").arg(static_cast<int>(type)).arg(host).arg(port); }
    QString address() const { return QStringLiteral("%1:%2").arg(host).arg(port); }
};

struct ProxySource {
    QString id;                                   // "src_<uuid>"
    QString name;
    QString url;                                  // в настройках — через DPAPI (в URL бывает ключ)
    ProxySlot::Type defaultType = ProxySlot::Type::Socks5;
    bool enabled = true;
    QVector<SourceEntry> entries;
    qint64 fetchedAt = 0;                         // последняя загрузка списка, секунды Unix
    int skipped = 0;                              // строк без поддерживаемого протокола

    // Состояние работы (не сохраняется).
    int okCount = 0;
    int badCount = 0;
    int inUseClients = 0;                         // окон через прокси этого источника
    int inUseProxies = 0;
    bool fetching = false;
    int checkGen = 0;                             // меняется — очередь проверок устарела
    int checkTotal = 0;
    int checkDone = 0;
    QString status;
    bool statusError = false;
    bool cacheDirty = false;

    bool checking() const { return checkDone < checkTotal; }

    void recount()
    {
        okCount = badCount = 0;
        for (const SourceEntry &e : entries) {
            okCount += e.state == SourceEntry::Ok;
            badCount += e.state == SourceEntry::Bad;
        }
    }
};

struct SourceParseResult {
    QVector<SourceEntry> entries;
    int skipped = 0;
};

// Разбор списка; дубликаты (тот же протокол, хост и порт) отбрасываются.
// Не больше maxEntries записей.
SourceParseResult parseSourceList(const QByteArray &text, ProxySlot::Type defaultType, int maxEntries);
