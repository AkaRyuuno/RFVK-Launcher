#pragma once
#include "ProxySlot.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

// Проверка прокси тем же путём, что у клиента: подключение, вход и CONNECT
// к серверу игры (target). Успех — только если прокси открыл соединение с
// сервером (SOCKS5 — ответ 0x00, HTTP — 200); сразу после этого соединение
// закрывается, данные игре не отправляются. Веб-сервер вместо HTTP-прокси
// или прокси без выхода к серверу игры проверку не пройдут.
// Итог — один сигнал finished(), затем объект удаляет себя сам.
class ProxyProbe : public QObject {
    Q_OBJECT
public:
    // Дедлайн всей проверки — как у рукопожатия в rfvk_proxy.dll.
    static constexpr int kDefaultTimeoutMs = 15000;

    ProxyProbe(const ProxySlot &proxy, const QHostAddress &target, quint16 targetPort,
               QObject *parent = nullptr, int timeoutMs = kDefaultTimeoutMs, int connectTimeoutMs = 0);
    // connectTimeoutMs > 0 — отдельный, более короткий дедлайн TCP-подключения
    // к самому прокси (мёртвые адреса массовой проверки отсеиваются быстрее).
    void start();

signals:
    // pingMs — время TCP-подключения к прокси (-1 — не подключились).
    void finished(bool ok, int pingMs, const QString &message);

private:
    void onConnected();
    void onReadyRead();
    void sendSocksConnect();
    void finish(bool ok, const QString &message);

    enum class Step { Connecting, SocksMethod, SocksAuth, SocksConnect, HttpStatus, Done };

    ProxySlot m_proxy;
    QHostAddress m_target;
    quint16 m_targetPort;
    QTcpSocket m_socket;
    QTimer m_timeout;
    QElapsedTimer m_clock;
    QByteArray m_buf;
    Step m_step = Step::Connecting;
    int m_pingMs = -1;
    int m_timeoutMs;
    int m_connectTimeoutMs;
};
