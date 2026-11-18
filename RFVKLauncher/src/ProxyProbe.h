#pragma once
#include "ProxySlot.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

// Проверка прокси: TCP-подключение и вход по протоколу, без соединения с
// внешними адресами.
//   SOCKS5 — приветствие и, если нужно, логин/пароль (RFC 1929); CONNECT не
//            отправляется.
//   HTTP   — CONNECT на 127.0.0.1:1 (адрес на стороне прокси): 407 — прокси
//            не принял логин; любой другой ответ HTTP — прокси жив и вход
//            принят (сам туннель к порту 1 прокси, разумеется, не откроет).
// Итог — один сигнал finished(), затем объект удаляет себя сам.
class ProxyProbe : public QObject {
    Q_OBJECT
public:
    ProxyProbe(const ProxySlot &proxy, QObject *parent = nullptr);
    void start();

signals:
    // pingMs — время TCP-подключения к прокси (-1 — не подключились).
    void finished(bool ok, int pingMs, const QString &message);

private:
    void onConnected();
    void onReadyRead();
    void finish(bool ok, const QString &message);

    enum class Step { Connecting, SocksMethod, SocksAuth, HttpStatus, Done };

    ProxySlot m_proxy;
    QTcpSocket m_socket;
    QTimer m_timeout;
    QElapsedTimer m_clock;
    QByteArray m_buf;
    Step m_step = Step::Connecting;
    int m_pingMs = -1;
};
