#pragma once
#include <QObject>
#include <QString>

// Автоподключение библиотеки улучшенного HUD (seirin.dll) к запущенному
// клиенту RF Online — опционально, по настройке пользователя.
//
// Библиотека НЕ входит в поставку: пользователь кладёт её рядом с лаунчером
// сам. Нет файла — подключение молча пропускается (dllAvailable() == false).
//
// Сам инжект делает 32-битный rfvk_inject.exe (разрядность клиента); лаунчер
// 64-битный и запустить CreateRemoteThread в x86-цель напрямую не может.
// HudInjector ждёт появления окна клиента (как при ручном инжекте в уже
// работающую игру) и затем зовёт инжектор через QProcess. Ожидание не
// блокирует очередь запусков. Любая неудача не срывает запуск игры.
class HudInjector : public QObject {
    Q_OBJECT
public:
    explicit HudInjector(QObject *parent = nullptr);

    // Путь к библиотеке/инжектору — рядом с исполняемым файлом лаунчера.
    static QString dllPath();
    static QString injectorPath();
    static bool dllFound();       // seirin.dll рядом с лаунчером
    static bool injectorFound();  // rfvk_inject.exe рядом с лаунчером (часть поставки)
    // Есть ли что подключать: присутствует и библиотека, и инжектор.
    static bool dllAvailable();

    // Дождаться видимого окна процесса pid и подключить к нему библиотеку.
    // Итог приходит сигналом log() (идёт в журнал лаунчера).
    void attach(qint64 pid, const QString &nick);

signals:
    // Строка для журнала: текст и уровень (0 — обычная, 1 — успех, 2 — ошибка).
    void log(const QString &text, int level);

private:
    // Ждёт появления видимого верхнеуровневого окна у pid; затем inject().
    void waitForWindow(qint64 pid, const QString &nick, int elapsedMs);
    void inject(qint64 pid, const QString &nick);
};
