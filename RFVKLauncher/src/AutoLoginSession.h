#pragma once
#include "AutoLoginStore.h"

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QTimer>

namespace rfvk_autologin { struct Shared; }

// Автовход одного запущенного клиента — сторона лаунчера.
//
// До подключения rfvk_autologin.dll к клиенту лаунчер создаёт именованную
// общую память «Local\RFVK.AutoLogin.<pid>» (формат — injector/
// rfvk_autologin_shared.h) и кладёт туда шаги и настройки входа аккаунта.
// Библиотека в клиенте сама проходит экраны входа и пишет обратно строки
// журнала и список персонажей; сессия опрашивает память по таймеру и
// пересылает это сигналами. FG-пароль библиотека затирает сразу после чтения.
class AutoLoginSession : public QObject {
    Q_OBJECT
public:
    struct Config {
        QString accountId;
        QString nick;
        QByteArray fg;        // пусто — FG вводит игрок
        QString charName;     // пусто — персонажа выбирает игрок
        int charSlot = -1;
        quint32 worldIndex = 0; // строка списка серверов: 0 — «Сервер 1»
    };

    AutoLoginSession(qint64 pid, Config config, QObject *parent = nullptr);
    ~AutoLoginSession() override;

    // Создать и заполнить общую память. false — не вышло (причина — в log()).
    bool open();
    // Библиотека не подключилась — дальше ждать нечего.
    void abandon();

    qint64 pid() const { return m_pid; }
    QString accountId() const { return m_cfg.accountId; }

signals:
    void log(const QString &text, int level);
    void charactersFound(const QString &accountId, const QList<SavedCharacter> &list);
    // Сессия закончилась (автовход завершён, клиент закрыт, тайм-аут) —
    // владелец удаляет объект.
    void ended(qint64 pid);

private:
    void poll();
    void drainEvents();
    void end();

    qint64 m_pid = 0;
    Config m_cfg;
    void *m_mapping = nullptr; // HANDLE
    rfvk_autologin::Shared *m_view = nullptr;
    QTimer m_timer;
    QElapsedTimer m_age;
    int m_seenEvents = 0;
    bool m_charsTaken = false;
    bool m_ended = false;
};
