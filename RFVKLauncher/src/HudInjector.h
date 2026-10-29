#pragma once
#include <QObject>
#include <QString>

#include <memory>

namespace remote_load {
class Job;
struct Result;
}

// Автоподключение библиотеки улучшенного HUD (seirin.dll) к запущенному
// клиенту RF Online — опционально, по настройке пользователя.
//
// Библиотека НЕ входит в поставку: пользователь кладёт её рядом с лаунчером
// сам. Нет файла — подключение молча пропускается (dllAvailable() == false).
//
// HudInjector ждёт появления окна клиента (как при ручном инжекте в уже
// работающую игру) и затем загружает библиотеку в 32-битный клиент прямо из
// 64-битного лаунчера (RemoteLoadLibrary.h). Ожидание не блокирует очередь
// запусков и интерфейс. Любая неудача не срывает запуск игры.
class HudInjector : public QObject {
    Q_OBJECT
public:
    explicit HudInjector(QObject *parent = nullptr);

    // Путь к библиотеке HUD — рядом с исполняемым файлом лаунчера.
    static QString dllPath();
    // Есть ли что подключать: seirin.dll лежит рядом с лаунчером.
    static bool dllAvailable();

    // Библиотека автовхода (часть поставки, 32-битный проект autologin/).
    static QString autoLoginDllPath();
    static bool autoLoginAvailable(); // rfvk_autologin.dll на месте

    // Дождаться видимого окна процесса pid и подключить к нему библиотеку HUD.
    // Итог приходит сигналом log() (идёт в журнал лаунчера).
    void attach(qint64 pid, const QString &nick);
    // То же для произвольной библиотеки; tag — подпись строк журнала («HUD»,
    // «Автовход»). Неудачу сообщает сигналом log() и failed(pid).
    void attachLibrary(qint64 pid, const QString &dll, const QString &tag, const QString &nick);

signals:
    // Строка для журнала: текст и уровень (0 — обычная, 1 — успех, 2 — ошибка).
    void log(const QString &text, int level);
    // Библиотека не подключена (клиент закрылся, окно не появилось, загрузка не удалась).
    void failed(qint64 pid, const QString &dll);

private:
    struct Job {
        qint64 pid = 0;
        QString dll;
        QString tag;
        QString nick;
    };
    // Ждёт появления видимого верхнеуровневого окна у pid; затем inject().
    void waitForWindow(const Job &job, int elapsedMs);
    void inject(const Job &job);
    // Итог шага загрузки: Pending — проверить снова позже; иначе — в журнал.
    void report(const Job &job, const remote_load::Result &r, const std::shared_ptr<remote_load::Job> &load);
};
