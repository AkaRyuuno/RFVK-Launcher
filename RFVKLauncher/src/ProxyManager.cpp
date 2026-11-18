#include "ProxyManager.h"
#include "ProxyProbe.h"
#include "RemoteLoadLibrary.h"
#include "VkCrypto.h"
#include "rfvk_proxy_shared.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtEndian>

#include <cstring>
#include <cwchar>
#include <string>
#include <utility>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace px = rfvk_proxy;

namespace {
constexpr int kPollMs = 250;                    // проверка клиентов и подтверждений
constexpr int kLoadPollMs = 50;                 // проверка потока LoadLibraryW
constexpr unsigned long kLoadTimeoutMs = 15000; // LoadLibraryW в клиенте — не дольше 15 с
constexpr int kStartRetryMs = 25;               // kernel32 в только что запущенном клиенте
constexpr qint64 kStartRetryWindowMs = 3000;    // появляется не сразу — повторяем до 3 с
constexpr qint64 kConfirmTimeoutMs = 20000;     // подтверждение от библиотеки

const QString kListKey = QStringLiteral("proxy/list");
const QString kAccountKey = QStringLiteral("proxy/account/");
const QString kPasswordKey = QStringLiteral("passwordEnc");
const QString kProbeKey = QStringLiteral("proxy/probeTarget");
// Сервер входа RF Online (VK Play) — пока клиент ни разу не шёл через прокси.
const QString kDefaultProbeTarget = QStringLiteral("212.233.94.210:27780");

// «a.b.c.d:port» → адрес и порт; false — строка не IPv4:порт.
bool parseTarget(const QString &text, QHostAddress &addr, quint16 &port)
{
    const int colon = text.lastIndexOf(QLatin1Char(':'));
    bool ok = false;
    const int p = text.mid(colon + 1).toInt(&ok);
    QHostAddress a(text.left(colon));
    if (colon < 0 || !ok || p <= 0 || p > 65535 || a.protocol() != QAbstractSocket::IPv4Protocol)
        return false;
    addr = a;
    port = static_cast<quint16>(p);
    return true;
}

ProxySlot directSlot()
{
    ProxySlot s;
    s.id = ProxySlot::directId();
    s.name = QStringLiteral("Прямой IP (без прокси)");
    s.lastStatus = QStringLiteral("Локальная сеть");
    return s;
}
} // namespace

ProxyManager::ProxyManager(QObject *parent)
    : QObject(parent),
      // Тот же INI, что у Controller: все настройки лаунчера в одном файле.
      m_settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("RFVK"),
                 QStringLiteral("RFVKLauncher"))
{
    loadSettings();
    loadSources();
    if (!parseTarget(m_settings.value(kProbeKey).toString(), m_probeTarget, m_probePort))
        parseTarget(kDefaultProbeTarget, m_probeTarget, m_probePort);
    m_pollTimer.setInterval(kPollMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &ProxyManager::poll);
}

ProxyManager::~ProxyManager()
{
#ifdef Q_OS_WIN
    // Клиенты продолжают работать: библиотека держит общую память сама.
    // Модель и сигналы не трогаем — интерфейс к этому моменту уже разрушен.
    for (int row = 0; row < m_sources.items().size(); ++row) {
        if (m_sources.at(row).cacheDirty)
            saveSourceCache(m_sources.ref(row));
    }
    for (const Session &s : std::as_const(m_sessions)) {
        if (s.view)
            UnmapViewOfFile(s.view);
        if (s.mapping)
            CloseHandle(static_cast<HANDLE>(s.mapping));
        if (s.process)
            CloseHandle(static_cast<HANDLE>(s.process));
    }
#endif
}

// Путь в нативном виде: LoadLibraryW в клиенте требует обратные слэши.
QString ProxyManager::proxyDllPath()
{
    return QDir::toNativeSeparators(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("rfvk_proxy.dll")));
}

bool ProxyManager::proxyDllAvailable()
{
    return QFileInfo::exists(proxyDllPath());
}

// ─────────────────────────── настройки ──────────────────────────────────────

void ProxyManager::loadSettings()
{
    QVector<ProxySlot> list{directSlot()};
    const QJsonArray arr = QJsonDocument::fromJson(m_settings.value(kListKey).toByteArray()).array();
    for (const QJsonValue &v : arr) {
        const QJsonObject obj = v.toObject();
        ProxySlot s = ProxySlot::fromJson(obj);
        if (s.id.isEmpty() || s.isDirect())
            continue;
        const QString hex = obj.value(kPasswordKey).toString();
        if (!hex.isEmpty())
            s.password = QString::fromUtf8(VkCrypto::dpapiUnprotect(QByteArray::fromHex(hex.toLatin1())));
        s.lastStatus = QStringLiteral("Не проверен");
        list.append(s);
    }
    m_model.setSlots(list);
}

void ProxyManager::saveSettings()
{
    QJsonArray arr;
    for (const ProxySlot &s : m_model.slotItems()) {
        if (s.isDirect() || s.isFromSource())
            continue;
        QJsonObject obj = s.toJson();
        if (s.authEnabled && !s.password.isEmpty())
            obj[kPasswordKey] = QString::fromLatin1(VkCrypto::dpapiProtect(s.password.toUtf8()).toHex());
        arr.append(obj);
    }
    m_settings.setValue(kListKey, QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QString ProxyManager::validate(int type, const QString &host, int port, bool auth,
                               const QString &user, const QString &pass)
{
    if (!ProxySlot::isProxyType(type))
        return QStringLiteral("Неизвестный протокол прокси");
    if (host.isEmpty())
        return QStringLiteral("Укажите адрес прокси");
    // Поля общей памяти с rfvk_proxy.dll — с завершающим нулём.
    if (host.toUtf8().size() >= px::kHostLen)
        return QStringLiteral("Слишком длинный адрес прокси");
    if (port <= 0 || port > 65535)
        return QStringLiteral("Некорректный порт (1–65535)");
    if (auth && user.isEmpty())
        return QStringLiteral("Укажите логин прокси");
    if (user.toUtf8().size() >= px::kAuthLen || pass.toUtf8().size() >= px::kAuthLen)
        return QStringLiteral("Логин и пароль — не длиннее %1 байт").arg(px::kAuthLen - 1);
    return {};
}

// ─────────────────────────── слоты ──────────────────────────────────────────

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
    // Проверенные адреса источников, ещё не выданные клиентам.
    for (const ProxySource &src : m_sources.items()) {
        if (src.enabled)
            count += qMax(0, src.okCount - src.inUseProxies);
    }
    return count;
}

QVariantList ProxyManager::slotOptions() const
{
    QVariantList list;
    for (const ProxySlot &s : m_model.slotItems()) {
        if (s.isFromSource())
            continue; // временный слот — аккаунт привязывается к источнику целиком
        QString label = QStringLiteral("%1 (%2) [%3]").arg(s.name, s.typeName(), s.capacityString());
        if (s.isFull())
            label += QStringLiteral(" — заполнен");
        else if (!s.enabled)
            label += QStringLiteral(" — выключен");
        list.append(QVariantMap{{QStringLiteral("label"), label}, {QStringLiteral("value"), s.id}});
    }
    for (const ProxySource &src : m_sources.items()) {
        QString label = QStringLiteral("Источник «%1» [рабочих: %2]").arg(src.name).arg(src.okCount);
        if (!src.enabled)
            label += QStringLiteral(" — выключен");
        list.append(QVariantMap{{QStringLiteral("label"), label},
                                {QStringLiteral("value"), QStringLiteral("source:") + src.id}});
    }
    return list;
}

QString ProxyManager::accountPreferredSlot(const QString &accountId) const
{
    if (accountId.isEmpty())
        return QStringLiteral("auto");
    return m_settings.value(kAccountKey + accountId, QStringLiteral("auto")).toString();
}

void ProxyManager::setAccountPreferredSlot(const QString &accountId, const QString &slotId)
{
    if (accountId.isEmpty())
        return;
    if (slotId.isEmpty() || slotId == QLatin1String("auto"))
        m_settings.remove(kAccountKey + accountId);
    else
        m_settings.setValue(kAccountKey + accountId, slotId);
}

QString ProxyManager::allocateSlot(const QString &preferredSlotId) const
{
    if (preferredSlotId.startsWith(QLatin1String("source:"))) {
        const int row = m_sources.indexOfId(preferredSlotId.mid(7));
        if (row >= 0) {
            if (const QString id = allocateFromSource(m_sources.at(row)); !id.isEmpty())
                return id;
        }
    } else if (!preferredSlotId.isEmpty() && preferredSlotId != QLatin1String("auto")) {
        const int idx = m_model.indexOfId(preferredSlotId);
        if (idx >= 0) {
            const ProxySlot &s = m_model.at(idx);
            if (s.enabled && !s.isFull())
                return s.id;
        }
    }
    for (const ProxySlot &s : m_model.slotItems()) {
        // Порядок модели: прямой IP первым, затем прокси в порядке добавления.
        // Слоты источников добивает allocateFromSource().
        if (!s.isFromSource() && s.enabled && !s.isFull())
            return s.id;
    }
    for (const ProxySource &src : m_sources.items()) {
        if (const QString id = allocateFromSource(src); !id.isEmpty())
            return id;
    }
    return {};
}

QString ProxyManager::addProxy(const QString &name, int type, const QString &host, int port,
                               bool auth, const QString &user, const QString &pass)
{
    const QString cleanHost = host.trimmed();
    const QString cleanUser = user.trimmed();
    if (const QString err = validate(type, cleanHost, port, auth, cleanUser, pass); !err.isEmpty()) {
        emit toast(err, 2);
        return {};
    }

    ProxySlot s;
    s.id = QStringLiteral("proxy_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.type = static_cast<ProxySlot::Type>(type);
    s.host = cleanHost;
    s.port = static_cast<quint16>(port);
    s.name = name.trimmed().isEmpty() ? (s.typeName() + QLatin1Char(' ') + cleanHost) : name.trimmed();
    s.authEnabled = auth;
    s.username = auth ? cleanUser : QString();
    s.password = auth ? pass : QString();
    s.lastStatus = QStringLiteral("Не проверен");

    m_model.addSlot(s);
    saveSettings();
    emit slotsChanged();
    emit toast(QStringLiteral("Прокси «%1» добавлен").arg(s.name), 1);
    emit log(QStringLiteral("Добавлен прокси: %1 (%2:%3)").arg(s.name, s.host).arg(s.port), 0);
    testProxy(s.id);
    return s.id;
}

bool ProxyManager::updateProxy(const QString &id, const QString &name, int type, const QString &host,
                               int port, bool auth, const QString &user, const QString &pass)
{
    const int idx = m_model.indexOfId(id);
    if (idx < 0 || m_model.at(idx).isDirect() || m_model.at(idx).isFromSource())
        return false;

    ProxySlot s = m_model.at(idx);
    const QString cleanHost = host.trimmed();
    const QString cleanUser = user.trimmed();
    const QString newPass = pass.isEmpty() ? s.password : pass;
    if (const QString err = validate(type, cleanHost, port, auth, cleanUser, newPass); !err.isEmpty()) {
        emit toast(err, 2);
        return false;
    }

    if (!name.trimmed().isEmpty())
        s.name = name.trimmed();
    s.type = static_cast<ProxySlot::Type>(type);
    s.host = cleanHost;
    s.port = static_cast<quint16>(port);
    s.authEnabled = auth;
    s.username = auth ? cleanUser : QString();
    s.password = auth ? newPass : QString();
    s.pingMs = -1;
    s.testOk = true;
    s.lastStatus = QStringLiteral("Не проверен");

    m_model.updateSlot(idx, s);
    saveSettings();
    emit slotsChanged();
    emit toast(QStringLiteral("Прокси «%1» обновлён").arg(s.name), 1);
    // Уже запущенные клиенты работают со старыми настройками до перезапуска.
    if (s.activeClients() > 0)
        emit log(QStringLiteral("Прокси «%1» изменён — запущенные через него окна (%2) используют прежние "
                                "настройки до перезапуска").arg(s.name).arg(s.activeClients()), 0);
    testProxy(s.id);
    return true;
}

void ProxyManager::removeProxy(const QString &id)
{
    const int idx = m_model.indexOfId(id);
    if (idx < 0 || m_model.at(idx).isDirect() || m_model.at(idx).isFromSource())
        return;

    const ProxySlot s = m_model.at(idx);
    if (s.activeClients() > 0) {
        emit toast(QStringLiteral("Нельзя удалить прокси, пока через него запущены окна (%1)")
                       .arg(s.capacityString()), 2);
        return;
    }
    m_model.removeSlot(idx);
    saveSettings();
    emit slotsChanged();
    emit toast(QStringLiteral("Прокси «%1» удалён").arg(s.name), 1);
}

void ProxyManager::setProxyEnabled(const QString &id, bool enabled)
{
    const int idx = m_model.indexOfId(id);
    if (idx < 0 || m_model.at(idx).isDirect() || m_model.at(idx).isFromSource())
        return;
    ProxySlot s = m_model.at(idx);
    if (s.enabled == enabled)
        return;
    s.enabled = enabled;
    m_model.updateSlot(idx, s);
    saveSettings();
    emit slotsChanged();
}

// ─────────────────────────── проверка прокси ────────────────────────────────

void ProxyManager::runProbe(const ProxySlot &proxy, const std::function<void(bool, int, const QString &)> &done)
{
    auto *probe = new ProxyProbe(proxy, m_probeTarget, m_probePort, this);
    connect(probe, &ProxyProbe::finished, this, done);
    probe->start();
}

void ProxyManager::testProxy(const QString &id)
{
    const int idx = m_model.indexOfId(id);
    if (idx < 0 || m_model.at(idx).isDirect())
        return;

    ProxySlot s = m_model.at(idx);
    s.lastStatus = QStringLiteral("Проверка…");
    m_model.updateSlot(idx, s);

    runProbe(s, [this, id](bool ok, int pingMs, const QString &message) {
        setSlotHealth(id, ok, ok ? pingMs : -1,
                      ok ? QStringLiteral("Доступен · %1").arg(message)
                         : QStringLiteral("Недоступен: %1").arg(message));
        emit testFinished(id, ok, pingMs, message);
    });
}

void ProxyManager::testCustomProxy(int type, const QString &host, int port, bool auth,
                                   const QString &user, const QString &pass, const QString &existingId)
{
    QString usePass = pass;
    if (usePass.isEmpty() && !existingId.isEmpty()) {
        const int idx = m_model.indexOfId(existingId);
        if (idx >= 0)
            usePass = m_model.at(idx).password;
    }
    const QString cleanHost = host.trimmed();
    const QString cleanUser = user.trimmed();
    if (const QString err = validate(type, cleanHost, port, auth, cleanUser, usePass); !err.isEmpty()) {
        emit customTestFinished(false, -1, err);
        return;
    }

    ProxySlot s;
    s.type = static_cast<ProxySlot::Type>(type);
    s.host = cleanHost;
    s.port = static_cast<quint16>(port);
    s.authEnabled = auth;
    s.username = cleanUser;
    s.password = usePass;
    runProbe(s, [this](bool ok, int pingMs, const QString &message) {
        emit customTestFinished(ok, pingMs, message);
    });
}

// ─────────────────────────── клиенты игры ───────────────────────────────────

QString ProxyManager::slotName(const QString &slotId) const
{
    const int idx = m_model.indexOfId(slotId);
    return idx >= 0 ? m_model.at(idx).name : slotId;
}

bool ProxyManager::registerProcess(qint64 pid, const QString &slotId, const QString &nick)
{
#ifdef Q_OS_WIN
    const int idx = m_model.indexOfId(slotId);
    // Прокси из источника становится слотом с первым своим окном.
    ProxySlot slot = idx >= 0 ? m_model.at(idx) : makeSourceSlot(slotId);
    if (pid <= 0 || slot.id.isEmpty() || m_sessions.contains(pid)) {
        if (slot.id.isEmpty() && pid > 0 && slotId != ProxySlot::directId()) {
            // Адрес пропал из источника между выбором и запуском — без прокси не играем.
            if (HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid))) {
                TerminateProcess(h, 1);
                CloseHandle(h);
            }
            emit log(QStringLiteral("Клиент %1 закрыт: прокси из источника больше недоступен").arg(nick), 2);
        }
        return false;
    }

    Session sess;
    sess.pid = pid;
    sess.slotId = slotId;
    sess.nick = nick;
    sess.started.start();
    // SYNCHRONIZE — ждать завершения, TERMINATE — закрыть при сбое прокси.
    sess.process = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION,
                               FALSE, static_cast<DWORD>(pid));
    if (!sess.process) {
        emit log(QStringLiteral("Слоты: нет доступа к клиенту (PID %1, код %2) — окно не учтено")
                     .arg(pid).arg(GetLastError()), 2);
        return false;
    }

    auto fail = [&](const QString &reason) {
        TerminateProcess(static_cast<HANDLE>(sess.process), 1);
        if (sess.view)
            UnmapViewOfFile(sess.view);
        if (sess.mapping)
            CloseHandle(static_cast<HANDLE>(sess.mapping));
        CloseHandle(static_cast<HANDLE>(sess.process));
        const QString msg = QStringLiteral("Клиент %1 закрыт: %2").arg(nick, reason);
        emit log(msg, 2);
        emit toast(msg, 2);
        return false;
    };

    if (!slot.isDirect()) {
        if (!proxyDllAvailable())
            return fail(QStringLiteral("rfvk_proxy.dll не найдена рядом с лаунчером"));

        const std::wstring mapName = std::wstring(px::kMappingPrefix) + std::to_wstring(pid);
        HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                        sizeof(px::SharedConfig), mapName.c_str());
        const DWORD mapErr = GetLastError();
        if (map && mapErr == ERROR_ALREADY_EXISTS) {
            // Кто-то создал отображение раньше нас — его содержимому верить нельзя.
            CloseHandle(map);
            return fail(QStringLiteral("общая память прокси уже занята другим процессом"));
        }
        if (!map)
            return fail(QStringLiteral("не удалось создать общую память (код %1)").arg(mapErr));
        sess.mapping = map;
        auto *cfg = static_cast<px::SharedConfig *>(
            MapViewOfFile(map, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(px::SharedConfig)));
        if (!cfg)
            return fail(QStringLiteral("не удалось отобразить общую память (код %1)").arg(GetLastError()));
        sess.view = cfg;

        std::memset(cfg, 0, sizeof(*cfg));
        cfg->magic = px::kMagic;
        cfg->version = px::kVersion;
        cfg->size = sizeof(*cfg);
        cfg->type = static_cast<std::uint32_t>(slot.type);
        // Длины проверены validate() — строки влезают вместе с нулём.
        const QByteArray host = slot.host.toUtf8();
        std::memcpy(cfg->proxyHost, host.constData(), qMin<qsizetype>(host.size(), px::kHostLen - 1));
        cfg->proxyPort = slot.port;
        if (slot.authEnabled) {
            const QByteArray user = slot.username.toUtf8();
            const QByteArray pass = slot.password.toUtf8();
            cfg->authEnabled = 1;
            std::memcpy(cfg->username, user.constData(), qMin<qsizetype>(user.size(), px::kAuthLen - 1));
            std::memcpy(cfg->password, pass.constData(), qMin<qsizetype>(pass.size(), px::kAuthLen - 1));
        }
        sess.load = std::make_shared<remote_load::Job>();
    } else {
        sess.confirmed = true;
    }

    slot.activePids.append(pid);
    if (idx >= 0)
        m_model.updateSlot(idx, slot);
    else
        m_model.addSlot(slot);
    m_sessions.insert(pid, sess);
    updateSourceUsage();
    m_pollTimer.start();
    emit slotsChanged();

    if (slot.isDirect()) {
        emit log(QStringLiteral("%1 · прямой IP · слот «%2»: %3").arg(nick, slot.name, slot.capacityString()), 0);
    } else {
        const QString from = slot.isFromSource()
            ? QStringLiteral(" из источника «%1» [%2]").arg(slot.sourceName, slot.capacityString())
            : QString();
        emit log(QStringLiteral("%1 · подключаю прокси «%2» (%3:%4)%5…")
                     .arg(nick, slot.name, slot.host).arg(slot.port).arg(from), 0);
        startInject(pid);
    }
    return true;
#else
    Q_UNUSED(pid)
    Q_UNUSED(slotId)
    Q_UNUSED(nick)
    return false;
#endif
}

void ProxyManager::startInject(qint64 pid)
{
#ifdef Q_OS_WIN
    auto it = m_sessions.find(pid);
    if (it == m_sessions.end())
        return;
    handleLoad(pid, remote_load::start(static_cast<unsigned long>(pid), proxyDllPath().toStdWString(), *it->load));
#endif
}

void ProxyManager::handleLoad(qint64 pid, const remote_load::Result &r)
{
#ifdef Q_OS_WIN
    auto it = m_sessions.find(pid);
    if (it == m_sessions.end())
        return; // клиент уже закрыт
    switch (r.step) {
    case remote_load::Step::Loaded:
        return; // дальше ждём hookStatus в poll()
    case remote_load::Step::Pending:
        QTimer::singleShot(kLoadPollMs, this, [this, pid] {
            auto it = m_sessions.find(pid);
            if (it != m_sessions.end())
                handleLoad(pid, remote_load::poll(*it->load, kLoadTimeoutMs));
        });
        return;
    case remote_load::Step::Failed:
        // В только что запущенном клиенте kernel32 появляется не сразу.
        if (it->started.elapsed() < kStartRetryWindowMs
            && WaitForSingleObject(static_cast<HANDLE>(it->process), 0) == WAIT_TIMEOUT) {
            it->load = std::make_shared<remote_load::Job>();
            QTimer::singleShot(kStartRetryMs, this, [this, pid] { startInject(pid); });
            return;
        }
        failSession(pid, QStringLiteral("rfvk_proxy.dll не подключилась: ") + QString::fromStdWString(r.message));
        return;
    }
#else
    Q_UNUSED(pid)
    Q_UNUSED(r)
#endif
}

void ProxyManager::poll()
{
#ifdef Q_OS_WIN
    const QList<qint64> pids = m_sessions.keys();
    for (qint64 pid : pids) {
        auto it = m_sessions.find(pid);
        if (it == m_sessions.end())
            continue;
        if (WaitForSingleObject(static_cast<HANDLE>(it->process), 0) != WAIT_TIMEOUT) {
            const QString nick = it->nick;
            const QString slotId = it->slotId;
            endSession(pid);
            const int idx = m_model.indexOfId(slotId);
            emit log(QStringLiteral("Окно %1 закрыто · слот «%2» освобождён: %3")
                         .arg(nick, slotName(slotId),
                              idx >= 0 ? m_model.at(idx).capacityString() : QString()), 0);
            continue;
        }
        checkSession(*it);
    }
    if (m_sessions.isEmpty())
        m_pollTimer.stop();
#endif
}

void ProxyManager::checkSession(Session &s)
{
    auto *cfg = static_cast<px::SharedConfig *>(s.view);
    if (!cfg)
        return;
    const QString status = QString::fromWCharArray(cfg->statusText,
                                                   static_cast<int>(wcsnlen(cfg->statusText, px::kStatusLen)));
    if (!s.confirmed) {
        switch (cfg->hookStatus) {
        case px::HookOk:
            s.confirmed = true;
            s.lastStatus = status;
            emit log(QStringLiteral("%1 · перехват сети установлен, соединения пойдут через «%2»")
                         .arg(s.nick, slotName(s.slotId)), 1);
            return;
        case px::HookFailed:
        case px::HookLeaked:
            failSession(s.pid, status.isEmpty() ? QStringLiteral("перехват сети не встал") : status);
            return;
        default:
            if (s.started.elapsed() > kConfirmTimeoutMs)
                failSession(s.pid, QStringLiteral("rfvk_proxy.dll не подтвердила перехват за %1 с")
                                       .arg(kConfirmTimeoutMs / 1000));
            return;
        }
    }
    // Сервер игры, к которому шёл клиент, — цель следующих проверок прокси.
    const quint32 target = cfg->targetAddr;
    if (target != 0 && target != s.lastTarget) {
        s.lastTarget = target;
        const QHostAddress addr(qFromBigEndian(target));
        const quint16 port = qFromBigEndian(static_cast<quint16>(cfg->targetPort));
        if (addr != m_probeTarget || port != m_probePort) {
            m_probeTarget = addr;
            m_probePort = port;
            m_settings.setValue(kProbeKey, QStringLiteral("%1:%2").arg(addr.toString()).arg(port));
        }
    }
    // События туннеля — в журнал (ошибки — красным) и в карточку слота.
    if (!status.isEmpty() && status != s.lastStatus) {
        s.lastStatus = status;
        const bool failed = cfg->tunnelState == px::TunnelError;
        emit log(s.nick + QStringLiteral(" · ") + status, failed ? 2 : 0);
        if (cfg->tunnelState != px::TunnelIdle)
            setSlotHealth(s.slotId, !failed, -1, failed ? QStringLiteral("Ошибка в игре: ") + status : status);
    }
}

void ProxyManager::setSlotHealth(const QString &slotId, bool ok, int pingMs, const QString &status)
{
    const int row = m_model.indexOfId(slotId);
    if (row < 0 || m_model.at(row).isDirect())
        return;
    ProxySlot s = m_model.at(row);
    s.testOk = ok;
    if (pingMs >= 0 || !ok)
        s.pingMs = pingMs;
    s.lastStatus = status;
    m_model.updateSlot(row, s);
    if (s.isFromSource())
        setSourceEntryHealth(s, ok, pingMs);
}

void ProxyManager::failSession(qint64 pid, const QString &reason)
{
#ifdef Q_OS_WIN
    auto it = m_sessions.find(pid);
    if (it == m_sessions.end())
        return;
    const QString nick = it->nick;
    TerminateProcess(static_cast<HANDLE>(it->process), 1);
    endSession(pid);
    const QString msg = QStringLiteral("Клиент %1 закрыт, чтобы не играть с реального IP: %2").arg(nick, reason);
    emit log(msg, 2);
    emit toast(msg, 2);
#else
    Q_UNUSED(pid)
    Q_UNUSED(reason)
#endif
}

void ProxyManager::endSession(qint64 pid)
{
#ifdef Q_OS_WIN
    const auto it = m_sessions.constFind(pid);
    if (it == m_sessions.constEnd())
        return;
    const Session s = *it;
    m_sessions.erase(it);
    if (s.view)
        UnmapViewOfFile(s.view);
    if (s.mapping)
        CloseHandle(static_cast<HANDLE>(s.mapping));
    if (s.process)
        CloseHandle(static_cast<HANDLE>(s.process));

    const int idx = m_model.indexOfId(s.slotId);
    if (idx >= 0) {
        ProxySlot slot = m_model.at(idx);
        if (slot.activePids.removeAll(pid) > 0) {
            // Слот источника живёт, пока через него запущено хоть одно окно.
            if (slot.isFromSource() && slot.activeClients() == 0)
                m_model.removeSlot(idx);
            else
                m_model.updateSlot(idx, slot);
            updateSourceUsage();
            emit slotsChanged();
        }
    }
#else
    Q_UNUSED(pid)
#endif
}
