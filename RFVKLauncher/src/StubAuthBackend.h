#pragma once
#include "IAuthBackend.h"
#include <QVector>
#include <functional>

// Заглушка-симулятор бэкенда: эмулирует шаги авторизации с задержками (QTimer),
// ведёт список тестовых аккаунтов и демонстрирует типовые ошибки
// (истёкшая сессия, отклонённая сессия). Реальной сети не делает. Список аккаунтов сохраняется в JSON
// рядом с данными приложения, чтобы добавленные/удалённые аккаунты
// переживали перезапуск.
class StubAuthBackend : public IAuthBackend {
    Q_OBJECT
public:
    explicit StubAuthBackend(QObject *parent = nullptr);

    void importSession() override;
    void launch(const QString &accountId) override;
    void refreshSession(const QString &accountId) override;
    void deleteAccount(const QString &accountId) override;
    QVector<Account> accounts() const override { return m_accounts; }

private:
    struct Step { QString text; int level; int delayMs; };

    // Последовательно проигрывает шаги (прогресс в лог), затем вызывает onDone.
    void runSteps(const QVector<Step> &steps, std::function<void()> onDone);
    void setBusy(bool b);
    int indexOfId(const QString &id) const;
    void persist() const;
    void load();
    void seed();
    QString storePath() const;

    QVector<Account> m_accounts;
    bool m_busy = false;
};
