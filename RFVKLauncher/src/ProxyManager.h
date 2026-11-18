#pragma once
#include "ProxyModel.h"
#include "ProxySourceModel.h"

#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVariantList>

#include <functional>
#include <memory>

class QNetworkAccessManager;

namespace remote_load {
class Job;
struct Result;
}

// Менеджер слотов маршрутизации (прямое подключение + SOCKS5/HTTP прокси).
//
// Хранит список прокси (пароли — через DPAPI), раздаёт слоты запускаемым
// клиентам (не больше kMaxClientsPerSlot на слот), следит за завершением
// клиентов по дескриптору процесса и подключает 32-битную rfvk_proxy.dll к
// клиентам, запущенным через прокси.
//
// Подключение через прокси — по принципу «закрыто при сбое»: клиент считается
// подтверждённым, только когда библиотека сообщила, что перехваты стоят и
// прямых соединений до них не было. Не загрузилась, перехват не встал, клиент
// успел соединиться напрямую или подтверждение не пришло за kConfirmTimeoutMs —
// лаунчер закрывает клиент, чтобы тот не играл с реального IP.
//
// Источники — списки прокси по URL (ProxySource): загружаются, проверяются
// пачкой (ProxyProbe, CONNECT к серверу игры) и дают прокси для авто-выбора,
// когда прямой IP и добавленные вручную прокси заполнены. Выданный прокси из
// источника становится временным слотом (до 5 окон на адрес) и исчезает
// с закрытием последнего окна.
class ProxyManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(ProxyModel *model READ model CONSTANT)
    Q_PROPERTY(ProxySourceModel *sources READ sources CONSTANT)
    Q_PROPERTY(int totalActiveClients READ totalActiveClients NOTIFY slotsChanged)
    Q_PROPERTY(int availableSlotsCount READ availableSlotsCount NOTIFY slotsChanged)
    Q_PROPERTY(bool proxyDllAvailable READ proxyDllAvailable CONSTANT)
    // Слоты для выпадающего списка: [{label, value}] без пункта «Авто» (его
    // добавляет интерфейс). Источники — значения "source:<id>".
    Q_PROPERTY(QVariantList slotOptions READ slotOptions NOTIFY slotsChanged)

public:
    explicit ProxyManager(QObject *parent = nullptr);
    ~ProxyManager() override;

    ProxyModel *model() { return &m_model; }
    ProxySourceModel *sources() { return &m_sources; }

    int totalActiveClients() const;
    int availableSlotsCount() const;
    QVariantList slotOptions() const;

    static QString proxyDllPath();
    static bool proxyDllAvailable();

    // Слот для нового клиента. preferredSlotId — слот из настроек аккаунта
    // ("auto" или пусто — авто): берётся, если он есть, включён и не заполнен;
    // "source:<id>" — только прокси этого источника. Авто — прямой IP, пока в
    // нём есть место, затем первый свободный прокси, затем источники. Все
    // заняты — пустая строка. Прокси из источника становится слотом в
    // registerProcess().
    Q_INVOKABLE QString allocateSlot(const QString &preferredSlotId = QString()) const;

    // Привязка аккаунта к слоту (в настройках; "auto" — авто-выбор).
    Q_INVOKABLE QString accountPreferredSlot(const QString &accountId) const;
    Q_INVOKABLE void setAccountPreferredSlot(const QString &accountId, const QString &slotId);

    // type — ProxySlot::Type (1 — SOCKS5, 2 — HTTP). В updateProxy пустой
    // pass оставляет сохранённый пароль; включённость меняет setProxyEnabled.
    Q_INVOKABLE QString addProxy(const QString &name, int type, const QString &host, int port,
                                 bool auth, const QString &user, const QString &pass);
    Q_INVOKABLE bool updateProxy(const QString &id, const QString &name, int type, const QString &host,
                                 int port, bool auth, const QString &user, const QString &pass);
    Q_INVOKABLE void removeProxy(const QString &id);
    Q_INVOKABLE void setProxyEnabled(const QString &id, bool enabled);

    // Проверка прокси (ProxyProbe): CONNECT к серверу игры — по умолчанию
    // известный сервер входа, затем последний, к которому шёл клиент.
    // testCustomProxy — для диалога; existingId — редактируемый прокси:
    // пустой pass берётся из него.
    Q_INVOKABLE void testProxy(const QString &id);
    Q_INVOKABLE void testCustomProxy(int type, const QString &host, int port, bool auth,
                                     const QString &user, const QString &pass,
                                     const QString &existingId = QString());

    // Источники. defaultType — протокол строк без схемы (1 — SOCKS5, 2 — HTTP).
    // Добавленный источник сразу загружается и проверяется.
    Q_INVOKABLE QString addSource(const QString &name, const QString &url, int defaultType);
    Q_INVOKABLE bool updateSource(const QString &id, const QString &name, const QString &url, int defaultType);
    Q_INVOKABLE void removeSource(const QString &id);
    Q_INVOKABLE void setSourceEnabled(const QString &id, bool enabled);
    // Скачать список заново (статусы совпавших адресов сохраняются) и проверить.
    Q_INVOKABLE void refreshSource(const QString &id);
    // Проверить все адреса источника; повторный вызов во время проверки — стоп.
    Q_INVOKABLE void checkSource(const QString &id);
    Q_INVOKABLE void stopSourceCheck(const QString &id);

    // Учесть запущенный клиент в слоте; для прокси — общая память и
    // rfvk_proxy.dll. false — клиент не учтён (для прокси он уже закрыт),
    // причина — в журнале.
    bool registerProcess(qint64 pid, const QString &slotId, const QString &nick);

signals:
    void log(const QString &text, int level);
    void toast(const QString &text, int level);
    void slotsChanged();
    void testFinished(const QString &id, bool ok, int pingMs, const QString &message);
    void customTestFinished(bool ok, int pingMs, const QString &message);

private:
    struct Session {
        qint64 pid = 0;
        QString slotId;
        QString nick;
        void *process = nullptr;  // HANDLE: держим, чтобы PID не переиспользовался
        void *mapping = nullptr;  // HANDLE общей памяти (только прокси)
        void *view = nullptr;     // rfvk_proxy::SharedConfig *
        std::shared_ptr<remote_load::Job> load;
        QElapsedTimer started;
        bool confirmed = false;   // прямой IP — сразу; прокси — по hookStatus
        QString lastStatus;       // последняя строка библиотеки, ушедшая в журнал
        quint32 lastTarget = 0;   // последний сервер игры из библиотеки (сетевой порядок)
    };

    void loadSettings();
    void saveSettings();
    void loadSources();
    void saveSources();
    QString sourceCachePath(const QString &id) const;
    void loadSourceCache(ProxySource &src) const;
    void saveSourceCache(ProxySource &src) const;
    void setSourceStatus(int row, const QString &text, bool error);
    static QString validateSource(const QString &url, int defaultType);
    QNetworkAccessManager *nam();
    void startFetch(int row, bool checkAfter);
    void startCheck(int row, bool onlyUnchecked);
    void pumpChecks();
    void finishCheck(int row, const QString &summary);
    // Прокси из источника для клиента: свободный слот этого источника или
    // проверенный адрес с лучшим пингом. Пусто — нет подходящих.
    QString allocateFromSource(const ProxySource &src) const;
    static QString sourceSlotId(const QString &sourceId, const SourceEntry &e);
    // Слот для id из allocateFromSource(); пустой id — адреса уже нет.
    ProxySlot makeSourceSlot(const QString &slotId) const;
    void updateSourceUsage();
    // Итог туннеля или проверки слота из источника — в запись источника.
    void setSourceEntryHealth(const ProxySlot &slot, bool ok, int pingMs);
    // Пусто — данные прокси годятся; иначе текст ошибки для пользователя.
    static QString validate(int type, const QString &host, int port, bool auth,
                            const QString &user, const QString &pass);

    void startInject(qint64 pid);
    void handleLoad(qint64 pid, const remote_load::Result &r);
    void poll();
    void checkSession(Session &s);
    // Закрыть клиент, который не удалось пустить через прокси.
    void failSession(qint64 pid, const QString &reason);
    // Убрать клиент из слота и освободить ресурсы (процесс не трогает).
    void endSession(qint64 pid);
    // Итог туннеля или проверки — в карточку слота.
    void setSlotHealth(const QString &slotId, bool ok, int pingMs, const QString &status);
    QString slotName(const QString &slotId) const;
    void runProbe(const ProxySlot &proxy, const std::function<void(bool, int, const QString &)> &done);

    struct CheckJob {
        QString sourceId;
        int gen = 0;
        int index = 0;
    };

    ProxyModel m_model;
    ProxySourceModel m_sources;
    QNetworkAccessManager *m_nam = nullptr;
    QList<CheckJob> m_checkQueue;
    int m_checksRunning = 0;
    QSettings m_settings;
    QTimer m_pollTimer;
    QHash<qint64, Session> m_sessions;
    QHostAddress m_probeTarget;  // сервер игры для проверки прокси
    quint16 m_probePort = 0;
};
