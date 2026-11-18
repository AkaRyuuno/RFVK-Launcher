// ProxyManager: источники прокси по URL — загрузка, массовая проверка,
// выдача прокси клиентам и кэш на диске.
#include "ProxyManager.h"
#include "ProxyProbe.h"
#include "VkCrypto.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QUuid>

namespace {
constexpr qint64 kMaxListBytes = 16 * 1024 * 1024; // список больше — не список прокси
constexpr int kMaxEntries = 50000;
constexpr int kFetchTimeoutMs = 30000;
// Массовая проверка: одновременно, дедлайн одной проверки и подключения к
// прокси. Мёртвые адреса (их в бесплатных списках большинство) отваливаются
// по короткому дедлайну подключения, а живым остаётся время на CONNECT.
constexpr int kBulkParallel = 256;
constexpr int kBulkTimeoutMs = 10000;
constexpr int kBulkConnectTimeoutMs = 4000;
constexpr int kCacheVersion = 1;

const QString kSourcesKey = QStringLiteral("proxy/sources");
const QString kSourcePrefix = QStringLiteral("src:");

qint64 nowSecs()
{
    return QDateTime::currentSecsSinceEpoch();
}

// Сменить состояние записи, поддерживая счётчики источника.
void setEntryState(ProxySource &src, SourceEntry &e, bool ok, int pingMs)
{
    src.okCount -= e.state == SourceEntry::Ok;
    src.badCount -= e.state == SourceEntry::Bad;
    e.state = ok ? SourceEntry::Ok : SourceEntry::Bad;
    src.okCount += ok;
    src.badCount += !ok;
    if (pingMs >= 0 || !ok)
        e.pingMs = ok ? pingMs : -1;
    e.checkedAt = nowSecs();
    src.cacheDirty = true;
}
} // namespace

// ─────────────────────────── хранение ───────────────────────────────────────

void ProxyManager::loadSources()
{
    const QJsonArray arr = QJsonDocument::fromJson(m_settings.value(kSourcesKey).toByteArray()).array();
    for (const QJsonValue &v : arr) {
        const QJsonObject obj = v.toObject();
        ProxySource src;
        src.id = obj.value(QStringLiteral("id")).toString();
        src.name = obj.value(QStringLiteral("name")).toString();
        src.url = QString::fromUtf8(VkCrypto::dpapiUnprotect(
            QByteArray::fromHex(obj.value(QStringLiteral("urlEnc")).toString().toLatin1())));
        const int type = obj.value(QStringLiteral("type")).toInt();
        if (src.id.isEmpty() || src.url.isEmpty() || !ProxySlot::isProxyType(type))
            continue;
        src.defaultType = static_cast<ProxySlot::Type>(type);
        src.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
        loadSourceCache(src);
        m_sources.add(src);
    }
}

void ProxyManager::saveSources()
{
    QJsonArray arr;
    for (const ProxySource &src : m_sources.items()) {
        arr.append(QJsonObject{
            {QStringLiteral("id"), src.id},
            {QStringLiteral("name"), src.name},
            {QStringLiteral("urlEnc"), QString::fromLatin1(VkCrypto::dpapiProtect(src.url.toUtf8()).toHex())},
            {QStringLiteral("type"), static_cast<int>(src.defaultType)},
            {QStringLiteral("enabled"), src.enabled},
        });
    }
    m_settings.setValue(kSourcesKey, QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

// Кэш списка и статусов — рядом с INI, через DPAPI: в строках бывают пароли.
QString ProxyManager::sourceCachePath(const QString &id) const
{
    return QFileInfo(m_settings.fileName()).absolutePath() + QStringLiteral("/proxy-sources/") + id
         + QStringLiteral(".dat");
}

void ProxyManager::loadSourceCache(ProxySource &src) const
{
    QFile f(sourceCachePath(src.id));
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(VkCrypto::dpapiUnprotect(f.readAll())).object();
    if (root.value(QStringLiteral("v")).toInt() != kCacheVersion)
        return;
    src.fetchedAt = root.value(QStringLiteral("fetchedAt")).toInteger();
    src.skipped = root.value(QStringLiteral("skipped")).toInt();
    const QJsonArray list = root.value(QStringLiteral("e")).toArray();
    src.entries.reserve(list.size());
    for (const QJsonValue &v : list) {
        const QJsonArray a = v.toArray();
        const int type = a.at(0).toInt();
        const int port = a.at(2).toInt();
        const int state = a.at(5).toInt();
        if (a.size() < 8 || !ProxySlot::isProxyType(type) || port <= 0 || port > 65535
            || state < SourceEntry::Unchecked || state > SourceEntry::Bad)
            continue;
        SourceEntry e;
        e.type = static_cast<ProxySlot::Type>(type);
        e.host = a.at(1).toString();
        e.port = static_cast<quint16>(port);
        e.username = a.at(3).toString();
        e.password = a.at(4).toString();
        e.state = static_cast<SourceEntry::State>(state);
        e.pingMs = a.at(6).toInt(-1);
        e.checkedAt = a.at(7).toInteger();
        src.entries.append(e);
    }
    src.recount();
}

void ProxyManager::saveSourceCache(ProxySource &src) const
{
    QJsonArray list;
    for (const SourceEntry &e : std::as_const(src.entries)) {
        list.append(QJsonArray{static_cast<int>(e.type), e.host, static_cast<int>(e.port), e.username,
                               e.password, static_cast<int>(e.state), e.pingMs, e.checkedAt});
    }
    const QJsonObject root{
        {QStringLiteral("v"), kCacheVersion},
        {QStringLiteral("fetchedAt"), src.fetchedAt},
        {QStringLiteral("skipped"), src.skipped},
        {QStringLiteral("e"), list},
    };
    const QString path = sourceCachePath(src.id);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)
        && f.write(VkCrypto::dpapiProtect(QJsonDocument(root).toJson(QJsonDocument::Compact))) >= 0
        && f.commit())
        src.cacheDirty = false;
}

// ─────────────────────────── управление ─────────────────────────────────────

QString ProxyManager::validateSource(const QString &url, int defaultType)
{
    const QUrl u(url, QUrl::StrictMode);
    if (!u.isValid() || u.host().isEmpty()
        || (u.scheme() != QLatin1String("http") && u.scheme() != QLatin1String("https")))
        return QStringLiteral("Укажите ссылку http:// или https:// на список прокси");
    if (!ProxySlot::isProxyType(defaultType))
        return QStringLiteral("Неизвестный протокол прокси");
    return {};
}

void ProxyManager::setSourceStatus(int row, const QString &text, bool error)
{
    ProxySource &src = m_sources.ref(row);
    src.status = text;
    src.statusError = error;
    m_sources.touch(row);
}

QString ProxyManager::addSource(const QString &name, const QString &url, int defaultType)
{
    const QString cleanUrl = url.trimmed();
    if (const QString err = validateSource(cleanUrl, defaultType); !err.isEmpty()) {
        emit toast(err, 2);
        return {};
    }
    ProxySource src;
    src.id = QStringLiteral("src_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    src.url = cleanUrl;
    src.name = name.trimmed().isEmpty() ? QUrl(cleanUrl).host() : name.trimmed();
    src.defaultType = static_cast<ProxySlot::Type>(defaultType);
    m_sources.add(src);
    saveSources();
    emit toast(QStringLiteral("Источник «%1» добавлен").arg(src.name), 1);
    emit log(QStringLiteral("Добавлен источник прокси: %1").arg(src.name), 0);
    startFetch(m_sources.indexOfId(src.id), true);
    return src.id;
}

bool ProxyManager::updateSource(const QString &id, const QString &name, const QString &url, int defaultType)
{
    const int row = m_sources.indexOfId(id);
    if (row < 0)
        return false;
    const QString cleanUrl = url.trimmed();
    if (const QString err = validateSource(cleanUrl, defaultType); !err.isEmpty()) {
        emit toast(err, 2);
        return false;
    }
    ProxySource &src = m_sources.ref(row);
    // Протокол строк без схемы известен только при разборе — перезагрузить.
    const bool refetch = src.url != cleanUrl || static_cast<int>(src.defaultType) != defaultType;
    if (!name.trimmed().isEmpty())
        src.name = name.trimmed();
    src.url = cleanUrl;
    src.defaultType = static_cast<ProxySlot::Type>(defaultType);
    m_sources.touch(row);
    saveSources();
    emit slotsChanged();
    emit toast(QStringLiteral("Источник «%1» обновлён").arg(src.name), 1);
    if (refetch)
        startFetch(row, true);
    return true;
}

void ProxyManager::removeSource(const QString &id)
{
    const int row = m_sources.indexOfId(id);
    if (row < 0)
        return;
    const ProxySource &src = m_sources.at(row);
    if (src.inUseClients > 0) {
        emit toast(QStringLiteral("Нельзя удалить источник, пока через его прокси запущены окна (%1)")
                       .arg(src.inUseClients), 2);
        return;
    }
    const QString name = src.name;
    QFile::remove(sourceCachePath(id));
    m_sources.remove(row); // очередь проверок этого источника отбросит pumpChecks()
    saveSources();
    emit slotsChanged();
    emit toast(QStringLiteral("Источник «%1» удалён").arg(name), 1);
}

void ProxyManager::setSourceEnabled(const QString &id, bool enabled)
{
    const int row = m_sources.indexOfId(id);
    if (row < 0 || m_sources.at(row).enabled == enabled)
        return;
    m_sources.ref(row).enabled = enabled;
    m_sources.touch(row);
    saveSources();
    emit slotsChanged();
}

void ProxyManager::refreshSource(const QString &id)
{
    const int row = m_sources.indexOfId(id);
    if (row >= 0)
        startFetch(row, true);
}

void ProxyManager::checkSource(const QString &id)
{
    const int row = m_sources.indexOfId(id);
    if (row >= 0)
        startCheck(row, false);
}

void ProxyManager::stopSourceCheck(const QString &id)
{
    const int row = m_sources.indexOfId(id);
    if (row < 0 || !m_sources.at(row).checking())
        return;
    ProxySource &src = m_sources.ref(row);
    const int done = src.checkDone;
    ++src.checkGen;
    src.checkTotal = src.checkDone = 0;
    finishCheck(row, QStringLiteral("Проверка остановлена на %1 · рабочих: %2").arg(done).arg(src.okCount));
}

// ─────────────────────────── загрузка ───────────────────────────────────────

QNetworkAccessManager *ProxyManager::nam()
{
    if (!m_nam)
        m_nam = new QNetworkAccessManager(this);
    return m_nam;
}

void ProxyManager::startFetch(int row, bool checkAfter)
{
    ProxySource &src = m_sources.ref(row);
    if (src.fetching)
        return;
    src.fetching = true;
    setSourceStatus(row, QStringLiteral("Загрузка списка…"), false);

    QNetworkRequest req{QUrl(src.url)};
    req.setTransferTimeout(kFetchTimeoutMs);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RFVKLauncher"));
    QNetworkReply *reply = nam()->get(req);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > kMaxListBytes) {
            reply->setProperty("tooBig", true);
            reply->abort();
        }
    });
    const QString id = src.id;
    connect(reply, &QNetworkReply::finished, this, [this, reply, id, checkAfter] {
        reply->deleteLater();
        const int row = m_sources.indexOfId(id);
        if (row < 0)
            return; // источник удалили, пока шла загрузка
        ProxySource &src = m_sources.ref(row);
        src.fetching = false;
        if (reply->property("tooBig").toBool())
            return setSourceStatus(row, QStringLiteral("Список больше %1 МБ — это не список прокси")
                                            .arg(kMaxListBytes / (1024 * 1024)), true);
        if (reply->error() != QNetworkReply::NoError)
            return setSourceStatus(row, QStringLiteral("Не удалось загрузить список: ") + reply->errorString(), true);

        SourceParseResult parsed = parseSourceList(reply->readAll(), src.defaultType, kMaxEntries);
        if (parsed.entries.isEmpty())
            return setSourceStatus(row, parsed.skipped > 0
                                            ? QStringLiteral("В списке нет SOCKS5 или HTTP прокси (пропущено строк: %1)")
                                                  .arg(parsed.skipped)
                                            : QStringLiteral("Список пуст или не похож на список прокси"),
                                   true);

        // Статусы адресов, которые были и в прошлой загрузке, сохраняются.
        QHash<QString, const SourceEntry *> old;
        for (const SourceEntry &e : std::as_const(src.entries))
            old.insert(e.key(), &e);
        int kept = 0;
        for (SourceEntry &e : parsed.entries) {
            if (const SourceEntry *prev = old.value(e.key())) {
                e.state = prev->state;
                e.pingMs = prev->pingMs;
                e.checkedAt = prev->checkedAt;
                kept += e.state != SourceEntry::Unchecked;
            }
        }
        ++src.checkGen; // индексы старого списка в очереди проверок больше не верны
        src.checkTotal = src.checkDone = 0;
        src.entries = std::move(parsed.entries);
        src.skipped = parsed.skipped;
        src.fetchedAt = nowSecs();
        src.recount();
        saveSourceCache(src);

        QString text = QStringLiteral("Загружено адресов: %1").arg(src.entries.size());
        if (src.skipped > 0)
            text += QStringLiteral(" · пропущено строк (SOCKS4 и др.): %1").arg(src.skipped);
        emit log(QStringLiteral("Источник «%1»: %2").arg(src.name, text), 0);
        setSourceStatus(row, text, false);
        emit slotsChanged();
        if (checkAfter)
            startCheck(row, kept > 0); // при обновлении — только новые адреса
    });
}

// ─────────────────────────── проверка ───────────────────────────────────────

void ProxyManager::startCheck(int row, bool onlyUnchecked)
{
    ProxySource &src = m_sources.ref(row);
    ++src.checkGen;
    src.checkDone = 0;
    src.checkTotal = 0;
    for (int i = 0; i < src.entries.size(); ++i) {
        if (onlyUnchecked && src.entries.at(i).state != SourceEntry::Unchecked)
            continue;
        m_checkQueue.append({src.id, src.checkGen, i});
        ++src.checkTotal;
    }
    if (src.checkTotal == 0) {
        setSourceStatus(row, src.entries.isEmpty() ? QStringLiteral("Список пуст — обновите его")
                                                   : QStringLiteral("Новых адресов нет · рабочих: %1").arg(src.okCount),
                        src.entries.isEmpty());
        return;
    }
    setSourceStatus(row, QStringLiteral("Проверка через сервер игры…"), false);
    emit log(QStringLiteral("Источник «%1»: проверяю адресов — %2").arg(src.name).arg(src.checkTotal), 0);
    pumpChecks();
}

void ProxyManager::pumpChecks()
{
    while (m_checksRunning < kBulkParallel && !m_checkQueue.isEmpty()) {
        const CheckJob job = m_checkQueue.takeFirst();
        const int row = m_sources.indexOfId(job.sourceId);
        if (row < 0)
            continue; // источник удалён
        const ProxySource &src = m_sources.at(row);
        if (src.checkGen != job.gen || job.index >= src.entries.size())
            continue; // проверку перезапустили или остановили
        const SourceEntry &e = src.entries.at(job.index);

        ProxySlot proxy;
        proxy.type = e.type;
        proxy.host = e.host;
        proxy.port = e.port;
        proxy.authEnabled = !e.username.isEmpty();
        proxy.username = e.username;
        proxy.password = e.password;
        auto *probe = new ProxyProbe(proxy, m_probeTarget, m_probePort, this, kBulkTimeoutMs,
                                    kBulkConnectTimeoutMs);
        ++m_checksRunning;
        connect(probe, &ProxyProbe::finished, this, [this, job](bool ok, int pingMs, const QString &) {
            --m_checksRunning;
            const int row = m_sources.indexOfId(job.sourceId);
            if (row >= 0) {
                ProxySource &src = m_sources.ref(row);
                if (src.checkGen == job.gen && job.index < src.entries.size()) {
                    setEntryState(src, src.entries[job.index], ok, pingMs);
                    if (++src.checkDone >= src.checkTotal)
                        finishCheck(row, QStringLiteral("Проверено: %1 · рабочих: %2")
                                             .arg(src.checkTotal).arg(src.okCount));
                    else
                        m_sources.markDirty(row);
                }
            }
            pumpChecks();
        });
        probe->start();
    }
}

void ProxyManager::finishCheck(int row, const QString &summary)
{
    ProxySource &src = m_sources.ref(row);
    src.checkTotal = src.checkDone = 0;
    saveSourceCache(src);
    emit log(QStringLiteral("Источник «%1»: %2").arg(src.name, summary), src.okCount > 0 ? 1 : 2);
    setSourceStatus(row, summary, src.okCount == 0);
    emit slotsChanged();
}

// ─────────────────────────── выдача клиентам ────────────────────────────────

QString ProxyManager::sourceSlotId(const QString &sourceId, const SourceEntry &e)
{
    return kSourcePrefix + sourceId + QLatin1Char('/') + e.key();
}

QString ProxyManager::allocateFromSource(const ProxySource &src) const
{
    if (!src.enabled)
        return {};
    // Сначала добить уже выданные адреса до 5 окон.
    QSet<QString> used;
    for (const ProxySlot &s : m_model.slotItems()) {
        if (s.sourceId == src.id && s.enabled && s.testOk && !s.isFull())
            return s.id;
        if (!s.isDirect())
            used.insert(QStringLiteral("%1:%2").arg(s.host.toLower()).arg(s.port));
    }
    // Иначе — проверенный адрес с лучшим пингом, которого ещё нет среди слотов.
    int best = -1;
    for (int i = 0; i < src.entries.size(); ++i) {
        const SourceEntry &e = src.entries.at(i);
        if (e.state != SourceEntry::Ok || used.contains(e.address().toLower()))
            continue;
        if (best < 0 || e.pingMs < src.entries.at(best).pingMs)
            best = i;
    }
    return best < 0 ? QString() : sourceSlotId(src.id, src.entries.at(best));
}

ProxySlot ProxyManager::makeSourceSlot(const QString &slotId) const
{
    ProxySlot s;
    if (!slotId.startsWith(kSourcePrefix))
        return s;
    const int slash = slotId.indexOf(QLatin1Char('/'), kSourcePrefix.size());
    const int row = m_sources.indexOfId(slotId.mid(kSourcePrefix.size(), slash - kSourcePrefix.size()));
    if (slash < 0 || row < 0)
        return s;
    const ProxySource &src = m_sources.at(row);
    const QString key = slotId.mid(slash + 1);
    for (const SourceEntry &e : src.entries) {
        if (e.key() != key)
            continue;
        s.id = slotId;
        s.name = e.address();
        s.type = e.type;
        s.host = e.host;
        s.port = e.port;
        s.authEnabled = !e.username.isEmpty();
        s.username = e.username;
        s.password = e.password;
        s.pingMs = e.pingMs;
        s.sourceId = src.id;
        s.sourceName = src.name;
        s.lastStatus = QStringLiteral("Проверен через сервер игры");
        return s;
    }
    return s;
}

void ProxyManager::updateSourceUsage()
{
    for (int row = 0; row < m_sources.items().size(); ++row) {
        int clients = 0;
        int proxies = 0;
        for (const ProxySlot &s : m_model.slotItems()) {
            if (s.sourceId == m_sources.at(row).id) {
                clients += s.activeClients();
                ++proxies;
            }
        }
        ProxySource &src = m_sources.ref(row);
        if (src.inUseClients != clients || src.inUseProxies != proxies) {
            src.inUseClients = clients;
            src.inUseProxies = proxies;
            m_sources.touch(row);
        }
    }
}

void ProxyManager::setSourceEntryHealth(const ProxySlot &slot, bool ok, int pingMs)
{
    const int row = m_sources.indexOfId(slot.sourceId);
    if (row < 0)
        return;
    ProxySource &src = m_sources.ref(row);
    for (SourceEntry &e : src.entries) {
        if (e.type == slot.type && e.host == slot.host && e.port == slot.port) {
            setEntryState(src, e, ok, pingMs);
            m_sources.markDirty(row);
            return;
        }
    }
}
