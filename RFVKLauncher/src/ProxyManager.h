#pragma once
#include "ProxyModel.h"
#include <QHash>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <memory>

namespace remote_load {
class Job;
struct Result;
}

// Менеджер слотов маршрутизации (прямое подключение + SOCKS5/HTTP прокси).
//
// Управляет списком прокси, их сохранением в QSettings, выделением слотов (до 5 окон
// на каждый слот), динамическим отслеживанием закрытия процессов игры и освобождением
// слотов, а также инжектом 32-битной rfvk_proxy.dll в клиентские процессы.
class ProxyManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(ProxyModel *model READ model CONSTANT)
    Q_PROPERTY(int totalActiveClients READ totalActiveClients NOTIFY totalClientsChanged)
    Q_PROPERTY(int availableSlotsCount READ availableSlotsCount NOTIFY availableSlotsCountChanged)
    Q_PROPERTY(bool proxyDllAvailable READ proxyDllAvailable CONSTANT)
    Q_PROPERTY(QVariantList slotOptions READ slotOptions NOTIFY slotsChanged)

public:
    explicit ProxyManager(QObject *parent = nullptr);
    ~ProxyManager() override;

    ProxyModel *model() { return &m_model; }
    const ProxyModel *model() const { return &m_model; }

    int totalActiveClients() const;
    int availableSlotsCount() const;
    QVariantList slotOptions() const;

    static QString proxyDllPath();
    static bool proxyDllAvailable();

    // Авто-распределение или получение слота с проверкой лимита (<= 5).
    // Если preferredSlotId задан и свободен (< 5) — возвращает его.
    // Если "auto" или пусто — берёт Слот 0 (Direct) если < 5, иначе первый свободный прокси.
    // Если все слоты заполнены (5/5) — возвращает пустую строку.
    Q_INVOKABLE QString allocateSlot(const QString &preferredSlotId = QString());

    // Настройка привязки конкретного аккаунта к слоту (хранится в QSettings).
    Q_INVOKABLE QString accountPreferredSlot(const QString &accountId) const;
    Q_INVOKABLE void setAccountPreferredSlot(const QString &accountId, const QString &slotId);

    // Добавление, редактирование, удаление прокси
    Q_INVOKABLE QString addProxy(const QString &name, int type, const QString &host, int port,
                                bool auth, const QString &user, const QString &pass);
    Q_INVOKABLE void updateProxy(const QString &id, const QString &name, int type, const QString &host,
                                int port, bool auth, const QString &user, const QString &pass, bool enabled);
    Q_INVOKABLE void removeProxy(const QString &id);
    Q_INVOKABLE void setProxyEnabled(const QString &id, bool enabled);

    // Тестирование прокси (измерение TCP задержки / пинга)
    Q_INVOKABLE void testProxy(const QString &id);
    Q_INVOKABLE void testCustomProxy(int type, const QString &host, int port,
                                    bool auth, const QString &user, const QString &pass);

    // Регистрация запущенного процесса игры: привязка к слоту, создание shared config,
    // и инжект rfvk_proxy.dll (если выбран прокси).
    bool registerProcess(qint64 pid, const QString &slotId, const QString &nick);

signals:
    void log(const QString &text, int level);
    void toast(const QString &text, int level);
    void totalClientsChanged();
    void availableSlotsCountChanged();
    void slotsChanged();
    void testFinished(const QString &id, bool ok, int pingMs, const QString &message);
    void customTestFinished(bool ok, int pingMs, const QString &message);

private slots:
    void pollProcesses();

private:
    void loadSettings();
    void saveSettings();
    void ensureDirectSlot();

    struct InjectJob {
        qint64 pid = 0;
        QString slotId;
        QString nick;
        std::shared_ptr<remote_load::Job> loadJob;
        int attempts = 0;
    };

    struct ProcessSession {
        qint64 pid = 0;
        QString slotId;
        QString nick;
        void *mappingHandle = nullptr;
        void *view = nullptr;
    };

    void pollInjectJob(InjectJob job);
    void cleanupProcessSession(qint64 pid);

    ProxyModel m_model;
    QSettings m_settings;
    QTimer m_pollTimer;
    QHash<qint64, ProcessSession> m_sessions;
};
