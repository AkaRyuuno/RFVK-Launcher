#include "ProxyManager.h"
#include "RemoteLoadLibrary.h"
#include "rfvk_proxy_shared.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUuid>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#ifdef Q_OS_WIN
bool isProcessRunning(qint64 pid)
{
    if (pid <= 0)
        return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h)
        return GetLastError() == ERROR_ACCESS_DENIED;
    DWORD code = 0;
    const bool ok = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return ok;
}
#else
bool isProcessRunning(qint64) { return false; }
#endif

} // namespace

ProxyManager::ProxyManager(QObject *parent)
    : QObject(parent)
{
    loadSettings();
    ensureDirectSlot();

    m_pollTimer.setInterval(1000); // Проверка завершения процессов каждую секунду
    connect(&m_pollTimer, &QTimer::timeout, this, &ProxyManager::pollProcesses);
    m_pollTimer.start();
}

ProxyManager::~ProxyManager()
{
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        cleanupProcessSession(it.key());
    }
    m_sessions.clear();
}

QString ProxyManager::proxyDllPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("rfvk_proxy.dll"));
}

bool ProxyManager::proxyDllAvailable()
{
    return QFileInfo::exists(proxyDllPath());
}

void ProxyManager::ensureDirectSlot()
{
    if (m_model.indexOfId(QStringLiteral("direct")) >= 0)
        return;

    ProxySlot direct;
    direct.id = QStringLiteral("direct");
    direct.name = QStringLiteral("Прямой IP (без прокси)");
    direct.type = 0;
    direct.maxClients = kMaxClientsPerSlot;
    direct.enabled = true;
    direct.lastStatus = QStringLiteral("Локальная сеть");

    QVector<ProxySlot> current = m_model.slotItems();
    current.prepend(direct);
    m_model.setSlots(current);
}

void ProxyManager::loadSettings()
{
    const QString jsonStr = m_settings.value(QStringLiteral("proxyList")).toString();
    QVector<ProxySlot> list;

    // Слот 0 — Прямой IP
    ProxySlot direct;
    direct.id = QStringLiteral("direct");
    direct.name = QStringLiteral("Прямой IP (без прокси)");
    direct.type = 0;
    direct.maxClients = kMaxClientsPerSlot;
    direct.enabled = true;
    direct.lastStatus = QStringLiteral("Локальная сеть");
    list.append(direct);

    if (!jsonStr.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (const QJsonValue &v : arr) {
                if (!v.isObject())
                    continue;
                ProxySlot s = ProxySlot::fromJson(v.toObject());
                if (s.id == QStringLiteral("direct") || s.id.isEmpty())
                    continue;
                s.maxClients = kMaxClientsPerSlot;
                s.activePids.clear();
                list.append(s);
            }
        }
    }

    m_model.setSlots(list);
}

void ProxyManager::saveSettings()
{
    QJsonArray arr;
    for (const ProxySlot &s : m_model.slotItems()) {
        if (s.isDirect())
            continue; // Слот direct всегда статический
        arr.append(s.toJson());
    }
    const QJsonDocument doc(arr);
    m_settings.setValue(QStringLiteral("proxyList"), QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

int ProxyManager::totalActiveClients() const
{
    int total = 0;
    for (const ProxySlot &s : m_model.slotItems())
        total += s.activeClients();
    return total;
}

int ProxyManager::availableSlotsCount() const
{
    int count = 0;
    for (const ProxySlot &s : m_model.slotItems()) {
        if (s.enabled && !s.isFull())
            ++count;
    }
    return count;
}

QVariantList ProxyManager::slotOptions() const
{
    QVariantList list;
    {
        QVariantMap m;
        m[QStringLiteral("label")] = QStringLiteral("Авто (прямой IP или свободный прокси)");
        m[QStringLiteral("value")] = QStringLiteral("auto");
        list.append(m);
    }
    for (const ProxySlot &s : m_model.slotItems()) {
        QVariantMap m;
        QString label = s.name + QStringLiteral(" (") + s.typeName() + QStringLiteral(") [") +
                        s.capacityString() + QStringLiteral("]");
        if (s.isFull())
            label += QStringLiteral(" — заполнен");
        m[QStringLiteral("label")] = label;
        m[QStringLiteral("value")] = s.id;
        list.append(m);
    }
    return list;
}

QString ProxyManager::accountPreferredSlot(const QString &accountId) const
{
    if (accountId.isEmpty())
        return QStringLiteral("auto");
    return m_settings.value(QStringLiteral("accountProxy/") + accountId, QStringLiteral("auto")).toString();
}

void ProxyManager::setAccountPreferredSlot(const QString &accountId, const QString &slotId)
{
    if (accountId.isEmpty())
        return;
    m_settings.setValue(QStringLiteral("accountProxy/") + accountId, slotId.isEmpty() ? QStringLiteral("auto") : slotId);
}

QString ProxyManager::allocateSlot(const QString &preferredSlotId)
{
    // 1. Если запрошен конкретный слот (не "auto")
    if (!preferredSlotId.isEmpty() && preferredSlotId != QStringLiteral("auto")) {
        const int idx = m_model.indexOfId(preferredSlotId);
        if (idx >= 0) {
            const ProxySlot &s = m_model.at(idx);
            if (!s.isFull() && (s.enabled || s.isDirect()))
                return s.id;
        }
    }

    // 2. Авто-выбор:
    // Сначала проверяем Слот 0 (Прямой IP) — если там меньше 5 клиентов, используем его!
    const int directIdx = m_model.indexOfId(QStringLiteral("direct"));
    if (directIdx >= 0 && !m_model.at(directIdx).isFull())
        return QStringLiteral("direct");

    // 3. Если на прямом IP уже 5/5 окон, ищем первый доступный прокси со слотом < 5
    for (const ProxySlot &s : m_model.slotItems()) {
        if (s.isDirect())
            continue;
        if (s.enabled && !s.isFull())
            return s.id;
    }

    // Все слоты заполнены!
    return {};
}

QString ProxyManager::addProxy(const QString &name, int type, const QString &host, int port,
                              bool auth, const QString &user, const QString &pass)
{
    const QString cleanHost = host.trimmed();
    if (cleanHost.isEmpty() || port <= 0 || port > 65535) {
        emit toast(QStringLiteral("Укажите корректный адрес и порт прокси"), 2);
        return {};
    }

    ProxySlot s;
    s.id = QStringLiteral("proxy_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.name = name.trimmed().isEmpty() ? (s.typeName() + QStringLiteral(" ") + cleanHost) : name.trimmed();
    s.type = type;
    s.host = cleanHost;
    s.port = static_cast<quint16>(port);
    s.authEnabled = auth;
    s.username = user.trimmed();
    s.password = pass;
    s.maxClients = kMaxClientsPerSlot;
    s.enabled = true;
    s.lastStatus = QStringLiteral("Не проверен");

    m_model.addSlot(s);
    saveSettings();
    emit availableSlotsCountChanged();
    emit slotsChanged();
    emit toast(QStringLiteral("Прокси «%1» добавлен").arg(s.name), 1);
    emit log(QStringLiteral("Добавлен прокси: %1 (%2:%3)").arg(s.name, s.host).arg(s.port), 0);

    // Автоматически запускаем пинг-проверку нового прокси
    testProxy(s.id);
    return s.id;
}

void ProxyManager::updateProxy(const QString &id, const QString &name, int type, const QString &host,
                              int port, bool auth, const QString &user, const QString &pass, bool enabled)
{
    const int idx = m_model.indexOfId(id);
    if (idx < 0 || m_model.at(idx).isDirect())
        return;

    ProxySlot s = m_model.at(idx);
    s.name = name.trimmed().isEmpty() ? s.name : name.trimmed();
    s.type = type;
    s.host = host.trimmed();
    s.port = static_cast<quint16>(port);
    s.authEnabled = auth;
    s.username = user.trimmed();
    s.password = pass;
    s.enabled = enabled;

    m_model.updateSlot(idx, s);
    saveSettings();
    emit availableSlotsCountChanged();
    emit slotsChanged();
    emit toast(QStringLiteral("Прокси «%1» обновлен").arg(s.name), 1);
}

void ProxyManager::removeProxy(const QString &id)
{
    const int idx = m_model.indexOfId(id);
    if (idx < 0 || m_model.at(idx).isDirect())
        return;

    const ProxySlot s = m_model.at(idx);
    if (s.activeClients() > 0) {
        emit toast(QStringLiteral("Нельзя удалить прокси, пока в нём есть активные окна (%1)").arg(s.capacityString()), 2);
        return;
    }

    m_model.removeSlot(idx);
    saveSettings();
    emit availableSlotsCountChanged();
    emit slotsChanged();
    emit toast(QStringLiteral("Прокси «%1» удалён").arg(s.name), 1);
}

void ProxyManager::setProxyEnabled(const QString &id, bool enabled)
{
    const int idx = m_model.indexOfId(id);
    if (idx < 0 || m_model.at(idx).isDirect())
        return;

    ProxySlot s = m_model.at(idx);
    if (s.enabled == enabled)
        return;
    s.enabled = enabled;
    m_model.updateSlot(idx, s);
    saveSettings();
    emit availableSlotsCountChanged();
    emit slotsChanged();
}

void ProxyManager::testProxy(const QString &id)
{
    const int idx = m_model.indexOfId(id);
    if (idx < 0)
        return;

    const ProxySlot s = m_model.at(idx);
    if (s.isDirect()) {
        ProxySlot upd = s;
        upd.pingMs = 0;
        upd.lastStatus = QStringLiteral("Локальное подключение");
        m_model.updateSlot(idx, upd);
        emit testFinished(id, true, 0, upd.lastStatus);
        return;
    }

    auto *socket = new QTcpSocket(this);
    auto *timer = new QElapsedTimer();
    timer->start();

    connect(socket, &QTcpSocket::connected, this, [this, id, socket, timer]() {
        const int elapsed = static_cast<int>(timer->restart());
        const int row = m_model.indexOfId(id);
        if (row >= 0) {
            ProxySlot s = m_model.at(row);
            s.pingMs = elapsed;
            s.lastStatus = QStringLiteral("Доступен (%1 мс)").arg(elapsed);
            m_model.updateSlot(row, s);
        }
        emit testFinished(id, true, elapsed, QStringLiteral("Доступен (%1 мс)").arg(elapsed));
        delete timer;
        socket->disconnectFromHost();
        socket->deleteLater();
    });

    connect(socket, &QTcpSocket::errorOccurred, this, [this, id, socket, timer](QAbstractSocket::SocketError) {
        const QString err = socket->errorString();
        const int row = m_model.indexOfId(id);
        if (row >= 0) {
            ProxySlot s = m_model.at(row);
            s.pingMs = -1;
            s.lastStatus = QStringLiteral("Недоступен: %1").arg(err);
            m_model.updateSlot(row, s);
        }
        emit testFinished(id, false, -1, err);
        delete timer;
        socket->deleteLater();
    });

    socket->connectToHost(s.host, s.port);
    QTimer::singleShot(4000, socket, [socket]() {
        if (socket->state() != QAbstractSocket::ConnectedState)
            socket->abort();
    });
}

void ProxyManager::testCustomProxy(int /*type*/, const QString &host, int port,
                                  bool /*auth*/, const QString &/*user*/, const QString &/*pass*/)
{
    const QString cleanHost = host.trimmed();
    if (cleanHost.isEmpty() || port <= 0 || port > 65535) {
        emit customTestFinished(false, -1, QStringLiteral("Неверный хост или порт"));
        return;
    }

    auto *socket = new QTcpSocket(this);
    auto *timer = new QElapsedTimer();
    timer->start();

    connect(socket, &QTcpSocket::connected, this, [this, socket, timer]() {
        const int elapsed = static_cast<int>(timer->restart());
        emit customTestFinished(true, elapsed, QStringLiteral("Соединение установлено (%1 мс)").arg(elapsed));
        delete timer;
        socket->disconnectFromHost();
        socket->deleteLater();
    });

    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket, timer](QAbstractSocket::SocketError) {
        const QString err = socket->errorString();
        emit customTestFinished(false, -1, QStringLiteral("Ошибка подключения: %1").arg(err));
        delete timer;
        socket->deleteLater();
    });

    socket->connectToHost(cleanHost, static_cast<quint16>(port));
    QTimer::singleShot(4000, socket, [socket]() {
        if (socket->state() != QAbstractSocket::ConnectedState)
            socket->abort();
    });
}

bool ProxyManager::registerProcess(qint64 pid, const QString &slotId, const QString &nick)
{
    const int idx = m_model.indexOfId(slotId);
    if (idx < 0)
        return false;

    ProxySlot s = m_model.at(idx);
    if (s.isFull()) {
        emit toast(QStringLiteral("Слот «%1» переполнен (максимум %2 окон)").arg(s.name).arg(kMaxClientsPerSlot), 2);
        return false;
    }

    s.activePids.append(pid);
    m_model.updateSlot(idx, s);
    emit totalClientsChanged();
    emit slotsChanged();

    // 1. Если выбран прямой IP
    if (s.isDirect()) {
        emit log(QStringLiteral("Клиент RF Online (PID %1, %2) запущен напрямую · Слот «%3»: %4")
                     .arg(pid).arg(nick, s.name, s.capacityString()),
                 0);
        return true;
    }

    // 2. Если выбран прокси — настраиваем общую память и инжектим rfvk_proxy.dll
#ifdef Q_OS_WIN
    const std::wstring mapName = std::wstring(rfvk_proxy::kMappingPrefix) + std::to_wstring(pid);
    HANDLE hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                     sizeof(rfvk_proxy::SharedConfig), mapName.c_str());
    if (!hMap) {
        emit log(QStringLiteral("Сбой создания общей памяти прокси для PID %1 (код %2)").arg(pid).arg(GetLastError()), 2);
        return false;
    }

    auto *cfg = static_cast<rfvk_proxy::SharedConfig *>(
        MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(rfvk_proxy::SharedConfig)));
    if (!cfg) {
        CloseHandle(hMap);
        emit log(QStringLiteral("Сбой маппинга памяти прокси для PID %1").arg(pid), 2);
        return false;
    }

    std::memset(cfg, 0, sizeof(*cfg));
    cfg->magic = rfvk_proxy::kMagic;
    cfg->version = rfvk_proxy::kVersion;
    cfg->size = sizeof(*cfg);
    cfg->type = static_cast<uint32_t>(s.type == 2 ? rfvk_proxy::ProxyType::HttpConnect : rfvk_proxy::ProxyType::Socks5);
    const QByteArray hostUtf8 = s.host.toUtf8();
    strncpy_s(cfg->proxyHost, hostUtf8.constData(), _TRUNCATE);
    cfg->proxyPort = s.port;
    cfg->authEnabled = s.authEnabled ? 1 : 0;
    if (s.authEnabled) {
        const QByteArray userUtf8 = s.username.toUtf8();
        const QByteArray passUtf8 = s.password.toUtf8();
        strncpy_s(cfg->username, userUtf8.constData(), _TRUNCATE);
        strncpy_s(cfg->password, passUtf8.constData(), _TRUNCATE);
    }
    cfg->state = rfvk_proxy::StateInit;

    ProcessSession sess;
    sess.pid = pid;
    sess.slotId = slotId;
    sess.nick = nick;
    sess.mappingHandle = hMap;
    sess.view = cfg;
    m_sessions.insert(pid, sess);

    // Запускаем инжект rfvk_proxy.dll
    const QString dllPath = proxyDllPath();
    if (!QFileInfo::exists(dllPath)) {
        emit log(QStringLiteral("rfvk_proxy.dll не найдена рядом с лаунчером (%1)").arg(dllPath), 2);
        return false;
    }

    InjectJob job;
    job.pid = pid;
    job.slotId = slotId;
    job.nick = nick;
    job.loadJob = std::make_shared<remote_load::Job>();
    job.attempts = 0;

    pollInjectJob(job);
#endif

    return true;
}

void ProxyManager::pollInjectJob(InjectJob job)
{
#ifdef Q_OS_WIN
    const QString dll = proxyDllPath();
    remote_load::Result r = remote_load::start(static_cast<DWORD>(job.pid), dll.toStdWString(), *job.loadJob);

    if (r.step == remote_load::Step::Loaded) {
        emit log(QStringLiteral("Туннель активирован · PID %1 (%2) · %3 [%4:%5]")
                     .arg(job.pid).arg(job.nick)
                     .arg(m_model.indexOfId(job.slotId) >= 0 ? m_model.at(m_model.indexOfId(job.slotId)).name : job.slotId)
                     .arg(m_model.indexOfId(job.slotId) >= 0 ? m_model.at(m_model.indexOfId(job.slotId)).host : QString())
                     .arg(m_model.indexOfId(job.slotId) >= 0 ? m_model.at(m_model.indexOfId(job.slotId)).port : 0),
                 1);
        return;
    }

    if (r.step == remote_load::Step::Pending) {
        // Поллинг завершения LoadLibraryW
        auto checkPending = [this, job]() mutable {
            remote_load::Result pollRes = remote_load::poll(*job.loadJob, 3000);
            if (pollRes.step == remote_load::Step::Loaded) {
                emit log(QStringLiteral("rfvk_proxy.dll загружена в процесс PID %1").arg(job.pid), 1);
            } else if (pollRes.step == remote_load::Step::Failed) {
                emit log(QStringLiteral("Ошибка загрузки rfvk_proxy.dll: %1").arg(QString::fromStdWString(pollRes.message)), 2);
            }
        };
        QTimer::singleShot(50, this, checkPending);
        return;
    }

    // Если процесс только что стартовал, kernel32 может быть не готов в первые 20-30 мс
    if (job.attempts < 15 && isProcessRunning(job.pid)) {
        ++job.attempts;
        QTimer::singleShot(25, this, [this, job]() {
            pollInjectJob(job);
        });
        return;
    }

    emit log(QStringLiteral("Не удалось подключить rfvk_proxy.dll к PID %1: %2")
                 .arg(job.pid).arg(QString::fromStdWString(r.message)),
             2);
#endif
}

void ProxyManager::cleanupProcessSession(qint64 pid)
{
#ifdef Q_OS_WIN
    if (m_sessions.contains(pid)) {
        ProcessSession sess = m_sessions.take(pid);
        if (sess.view)
            UnmapViewOfFile(sess.view);
        if (sess.mappingHandle)
            CloseHandle(static_cast<HANDLE>(sess.mappingHandle));
    }
#endif
}

void ProxyManager::pollProcesses()
{
    bool changed = false;

    for (int row = 0; row < m_model.rowCount(); ++row) {
        ProxySlot &s = m_model.slotRef(row);
        if (s.activePids.isEmpty())
            continue;

        QList<qint64> alive;
        for (qint64 pid : s.activePids) {
            if (isProcessRunning(pid)) {
                alive.append(pid);
            } else {
                cleanupProcessSession(pid);
                changed = true;
                emit log(QStringLiteral("Окно игры (PID %1) закрыто · Слот «%2» освобождён: %3/5")
                             .arg(pid).arg(s.name).arg(alive.size()),
                         0);
            }
        }

        if (alive.size() != s.activePids.size()) {
            s.activePids = alive;
            m_model.notifySlotChanged(row);
        }
    }

    if (changed) {
        emit totalClientsChanged();
    emit slotsChanged();
        emit availableSlotsCountChanged();
    emit slotsChanged();
    }
}
