#pragma once
#include "Account.h"
#include <QObject>
#include <QString>
#include <QVector>

// Абстрактный бэкенд авторизации. Все операции асинхронные: результат
// приходит сигналами. Реализации: VkPlayAuthBackend (настоящий) и
// StubAuthBackend (симулятор) — UI и контроллер зависят только от интерфейса.
// Операции выполняются по одной: пакетные действия над несколькими аккаунтами
// контроллер ставит в очередь и подаёт следующую после окончания предыдущей.
// Порядок сигналов операции: busyChanged(true) -> progress… ->
// finished/readyToLaunch/error -> busyChanged(false) (конец операции).
//
// Уровни строки прогресса/лога: 0 — обычная, 1 — успех, 2 — ошибка.
class IAuthBackend : public QObject {
    Q_OBJECT
public:
    explicit IAuthBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~IAuthBackend() override = default;

    // Импорт текущей сессии из установленного GameCenter (без пароля).
    virtual void importSession() = 0;
    // Полный путь запуска: обновить сессию -> получить код -> старт игры.
    virtual void launch(const QString &accountId) = 0;
    // Обновить сессию (refresh_token -> новый access/refresh).
    virtual void refreshSession(const QString &accountId) = 0;
    // Удалить аккаунт из лаунчера (с отзывом сессии на сервере, best-effort).
    virtual void deleteAccount(const QString &accountId) = 0;

    // Текущий снимок списка аккаунтов (для первичного заполнения модели).
    virtual QVector<Account> accounts() const = 0;

    // Каталог игры (нужен некоторым бэкендам для запуска). По умолчанию — no-op.
    virtual void setGameDir(const QString &) {}

signals:
    // Построчный прогресс операции — пишется в консоль лога.
    void progress(const QString &step, int level);
    // Номер текущего шага многошаговой операции (для счётчика «N/M» на кнопке).
    void stepProgress(int index, int total);
    // Конец операции: ok + человекочитаемое сообщение (идёт в тост).
    void finished(bool ok, const QString &message);
    // Список аккаунтов изменился — передаётся целиком.
    void accountsChanged(const QVector<Account> &items);
    // Сессия подтверждена и код получен — можно запускать клиент.
    // Фактический старт процесса RF_Online.exe делает контроллер (у него путь к игре).
    void readyToLaunch(const QString &accountId, const QString &persId, const QString &code);
    // UI блокируется на время длительной операции.
    void busyChanged(bool busy);
    // Понятный пользователю текст ошибки.
    void error(const QString &humanReadable);
};
