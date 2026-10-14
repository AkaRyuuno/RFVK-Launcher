#pragma once
#include "AccountModel.h"
#include "HudInjector.h"
#include "IAuthBackend.h"
#include <QHash>
#include <QList>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// Центральный контроллер UI: владеет моделью аккаунтов и бэкендом (через
// интерфейс IAuthBackend), транслирует сигналы бэкенда в строки лога и тосты,
// хранит выбор аккаунтов (один или несколько) и настройки (каталог игры, тема,
// выбор), делает локальную проверку окружения перед запуском игры.
//
// Действия «Запустить», «Обновить сессию», «Удалить» применяются ко всем
// выделенным аккаунтам: контроллер ставит их в очередь и подаёт бэкенду по
// одной операции (бэкенд выполняет операции последовательно).
class Controller : public QObject {
    Q_OBJECT
    Q_PROPERTY(AccountModel *accounts READ accounts CONSTANT)
    // Выбор: список id (в порядке списка), число выделенных, «основной» аккаунт
    // (для панели одного аккаунта) и сводка по всем выделенным (для мульти-панели).
    Q_PROPERTY(QStringList selectedIds READ selectedIds NOTIFY selectionChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY selectionChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedAccount READ selectedAccount NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectedAccounts READ selectedAccounts NOTIFY selectionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString progressStep READ progressStep NOTIFY progressChanged)
    Q_PROPERTY(int progressIndex READ progressIndex NOTIFY progressChanged)
    Q_PROPERTY(int progressTotal READ progressTotal NOTIFY progressChanged)
    // Пакет: номер текущей операции и их общее число (для «аккаунт 2/3»).
    Q_PROPERTY(int batchIndex READ batchIndex NOTIFY progressChanged)
    Q_PROPERTY(int batchTotal READ batchTotal NOTIFY progressChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)
    Q_PROPERTY(int accountCount READ accountCount NOTIFY accountsCountChanged)
    Q_PROPERTY(QString gameDir READ gameDir WRITE setGameDir NOTIFY gameStatusChanged)
    Q_PROPERTY(bool gameDirValid READ gameDirValid NOTIFY gameStatusChanged)
    Q_PROPERTY(bool gameCenterFound READ gameCenterFound NOTIFY gameStatusChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    // Автоподключение улучшенного HUD после запуска клиента (настройка).
    // hudAvailable — лежит ли библиотека рядом с лаунчером (иначе шаг пропускается).
    Q_PROPERTY(bool hudEnabled READ hudEnabled WRITE setHudEnabled NOTIFY hudChanged)
    Q_PROPERTY(bool hudAvailable READ hudAvailable NOTIFY hudChanged)
    // Что именно отсутствует (для текста предупреждения в настройках).
    Q_PROPERTY(bool hudDllFound READ hudDllFound NOTIFY hudChanged)
    Q_PROPERTY(bool hudInjectorFound READ hudInjectorFound NOTIFY hudChanged)

public:
    // Состояние сессии для QML: Controller.Active, Controller.Expiring и т.д.
    enum SessionState { None = 0, Active, Expiring, Expired };
    Q_ENUM(SessionState)

    Controller(AccountModel *model, IAuthBackend *backend, QObject *parent = nullptr);

    AccountModel *accounts() const { return m_model; }
    QStringList selectedIds() const { return m_selected; }
    int selectedCount() const { return m_selected.size(); }
    QString selectedId() const { return m_selected.isEmpty() ? QString() : m_selected.first(); }
    int selectedIndex() const { return m_model->indexOfId(selectedId()); }
    QVariantMap selectedAccount() const;
    QVariantList selectedAccounts() const;
    bool busy() const { return m_backendBusy || m_inFlight || !m_queue.isEmpty(); }
    QString progressStep() const { return m_progressStep; }
    int progressIndex() const { return m_progressIndex; }
    int progressTotal() const { return m_progressTotal; }
    int batchIndex() const { return m_batchIndex; }
    int batchTotal() const { return m_batchTotal; }
    // Текст последней ошибки выбранного аккаунта (только при одиночном выборе).
    QString errorText() const { return m_selected.size() == 1 ? m_errors.value(m_selected.first()) : QString(); }
    int accountCount() const { return m_model->rowCount(); }
    QString gameDir() const { return m_gameDir; }
    bool gameDirValid() const { return m_gameDirValid; }
    bool gameCenterFound() const { return m_gameCenterFound; }
    QString theme() const { return m_theme; }
    bool hudEnabled() const { return m_hudEnabled; }
    bool hudAvailable() const { return HudInjector::dllAvailable(); }
    bool hudDllFound() const { return HudInjector::dllFound(); }
    bool hudInjectorFound() const { return HudInjector::injectorFound(); }

public slots:
    // Выбор: одиночный (обычный клик), переключение (Ctrl+клик), диапазон
    // от последнего кликнутого (Shift+клик), все (Ctrl+A).
    void selectById(const QString &id);
    void toggleSelect(const QString &id);
    void selectRange(const QString &id);
    void selectAll();

    void importSession();
    void launch(const QString &id);
    void launchSelected();
    void refresh(const QString &id);
    void refreshSelected();
    void deleteAccount(const QString &id);
    void deleteSelected();
    void setGameDir(const QString &dir);
    void setTheme(const QString &t);
    void setHudEnabled(bool on);
    // Перечитать наличие seirin.dll/инжектора на диске (файл могли положить,
    // пока лаунчер открыт) — зовётся при открытии настроек.
    void refreshHudStatus() { emit hudChanged(); }
    void dismissError();

signals:
    void selectionChanged();
    void busyChanged();
    void accountsCountChanged();
    void progressChanged();
    void errorChanged();
    void gameStatusChanged();
    void themeChanged();
    void hudChanged();
    // Строка в консоль лога: время (ЧЧ:ММ:СС), текст, уровень (0/1/2).
    void logLine(const QString &time, const QString &text, int level);
    // Короткое уведомление-тост: текст, вид (0 инфо, 1 успех, 2 ошибка).
    void toast(const QString &text, int kind);

private:
    enum class OpKind { Import, Launch, Refresh, Delete };
    struct Op { OpKind kind; QString id; };

    void connectBackend();
    void recomputeGameStatus();
    void appendLog(const QString &text, int level);
    // Реальный старт процесса RF_Online.exe после подтверждения сессии бэкендом.
    void startGame(const QString &accountId, const QString &persId, const QString &code);

    // Очередь операций: enqueue -> pump подаёт бэкенду по одной -> opDone.
    void enqueue(OpKind kind, const QStringList &ids);
    void schedulePump();
    void pump();
    void opDone();
    void updateBusy();
    // Проверка каталога игры перед запуском (лог + тост при ошибке).
    bool checkGameDir();

    void setSelection(const QStringList &ids);
    QStringList inModelOrder(const QStringList &ids) const;
    void setError(const QString &id, const QString &msg);
    void clearError(const QString &id);
    QVariantMap accountMap(const Account &a) const;

    AccountModel *m_model = nullptr;
    IAuthBackend *m_backend = nullptr;
    HudInjector *m_hud = nullptr;
    QSettings m_settings;
    QStringList m_selected;          // выделенные id в порядке списка
    QString m_anchor;                // последний кликнутый (для Shift-диапазона)
    QString m_gameDir;
    QString m_theme = QStringLiteral("dark");
    bool m_hudEnabled = false;
    QString m_progressStep;
    int m_progressIndex = 0;
    int m_progressTotal = 0;
    QHash<QString, QString> m_errors; // id аккаунта -> текст последней ошибки

    QList<Op> m_queue;
    Op m_current{OpKind::Import, {}};
    bool m_inFlight = false;
    bool m_pumpScheduled = false;
    bool m_backendBusy = false;
    bool m_busyCache = false;
    int m_batchIndex = 0;
    int m_batchTotal = 0;
    int m_batchOk = 0;               // успешных операций в текущем пакете

    bool m_gameDirValid = false;
    bool m_gameCenterFound = false;
};
