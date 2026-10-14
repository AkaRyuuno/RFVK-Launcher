#pragma once
#include "IAuthBackend.h"
#include <QThread>

class VkWorker; // реализация в .cpp (живёт в рабочем потоке)

// Настоящий бэкенд VK Play на Qt (без внешних зависимостей и без Python).
// Повторяет протокол GameCenter: refresh_token -> access_token (o2-ac/token),
// gem.php?hint=Auth/Login -> профиль и код запуска; импорт без пароля — из
// SSO-cookie встроенного Chromium GameCenter (DPAPI + AES-GCM). Сетевые
// операции идут в рабочем потоке, результаты — сигналами (как у заглушки).
//
// Refresh-токены хранятся только здесь (DPAPI), в C++/QML не попадают;
// фактический старт RF_Online.exe делает контроллер по сигналу readyToLaunch.
class VkPlayAuthBackend : public IAuthBackend {
    Q_OBJECT
public:
    explicit VkPlayAuthBackend(QObject *parent = nullptr);
    ~VkPlayAuthBackend() override;

    void importSession() override;
    void launch(const QString &accountId) override;
    void refreshSession(const QString &accountId) override;
    void deleteAccount(const QString &accountId) override;
    QVector<Account> accounts() const override { return m_accounts; }
    void setGameDir(const QString &dir) override;

private:
    QThread m_thread;
    VkWorker *m_worker = nullptr;
    QVector<Account> m_accounts; // зеркало для синхронного accounts()
};
