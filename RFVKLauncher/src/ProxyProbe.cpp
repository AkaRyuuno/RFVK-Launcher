#include "ProxyProbe.h"

#include <QtEndian>

namespace {
constexpr int kTimeoutMs = 15000; // как дедлайн рукопожатия в rfvk_proxy.dll
constexpr int kMaxHttpHeader = 8192;

QString socksReply(quint8 rep)
{
    switch (rep) {
    case 0x01: return QStringLiteral("общая ошибка прокси");
    case 0x02: return QStringLiteral("соединение запрещено правилами прокси");
    case 0x03: return QStringLiteral("прокси: сеть недоступна");
    case 0x04: return QStringLiteral("прокси: сервер игры недоступен");
    case 0x05: return QStringLiteral("прокси: сервер игры отказал в соединении");
    case 0x06: return QStringLiteral("прокси: истекло время соединения с сервером игры");
    default: return QStringLiteral("прокси отказал в соединении (код %1)").arg(rep);
    }
}
} // namespace

ProxyProbe::ProxyProbe(const ProxySlot &proxy, const QHostAddress &target, quint16 targetPort,
                       QObject *parent)
    : QObject(parent), m_proxy(proxy), m_target(target), m_targetPort(targetPort)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        finish(false, m_step == Step::Connecting      ? QStringLiteral("прокси не отвечает")
                      : m_step == Step::SocksConnect || m_step == Step::HttpStatus
                          ? QStringLiteral("прокси не соединился с сервером игры за %1 с").arg(kTimeoutMs / 1000)
                          : QStringLiteral("прокси не ответил на вход"));
    });
    connect(&m_socket, &QTcpSocket::connected, this, &ProxyProbe::onConnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &ProxyProbe::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        finish(false, m_step == Step::Connecting ? m_socket.errorString()
                                                 : QStringLiteral("прокси закрыл соединение"));
    });
}

void ProxyProbe::start()
{
    m_clock.start();
    m_timeout.start(kTimeoutMs);
    m_socket.connectToHost(m_proxy.host, m_proxy.port);
}

void ProxyProbe::onConnected()
{
    m_pingMs = static_cast<int>(m_clock.elapsed());
    const bool withAuth = m_proxy.authEnabled && !m_proxy.username.isEmpty();
    if (m_proxy.type == ProxySlot::Type::Socks5) {
        m_step = Step::SocksMethod;
        m_socket.write(withAuth ? QByteArray("\x05\x02\x00\x02", 4) : QByteArray("\x05\x01\x00", 3));
        return;
    }
    m_step = Step::HttpStatus;
    const QByteArray hostPort = m_target.toString().toLatin1() + ':' + QByteArray::number(m_targetPort);
    QByteArray req = "CONNECT " + hostPort + " HTTP/1.1\r\nHost: " + hostPort + "\r\n";
    if (withAuth) {
        const QByteArray creds = m_proxy.username.toUtf8() + ':' + m_proxy.password.toUtf8();
        req += "Proxy-Authorization: Basic " + creds.toBase64() + "\r\n";
    }
    req += "\r\n";
    m_socket.write(req);
}

void ProxyProbe::sendSocksConnect()
{
    QByteArray req("\x05\x01\x00\x01", 4);
    const quint32 ip = qToBigEndian(m_target.toIPv4Address());
    const quint16 port = qToBigEndian(m_targetPort);
    req.append(reinterpret_cast<const char *>(&ip), 4);
    req.append(reinterpret_cast<const char *>(&port), 2);
    m_step = Step::SocksConnect;
    m_socket.write(req);
}

void ProxyProbe::onReadyRead()
{
    m_buf += m_socket.readAll();
    switch (m_step) {
    case Step::SocksMethod: {
        if (m_buf.size() < 2)
            return;
        if (static_cast<quint8>(m_buf[0]) != 0x05)
            return finish(false, QStringLiteral("сервер не SOCKS5"));
        const auto method = static_cast<quint8>(m_buf[1]);
        m_buf.remove(0, 2);
        if (method == 0x00)
            return sendSocksConnect();
        if (method != 0x02)
            return finish(false, QStringLiteral("прокси не принял ни один способ входа"));
        if (!m_proxy.authEnabled || m_proxy.username.isEmpty())
            return finish(false, QStringLiteral("прокси требует логин и пароль"));
        const QByteArray user = m_proxy.username.toUtf8();
        const QByteArray pass = m_proxy.password.toUtf8();
        if (user.size() > 255 || pass.size() > 255)
            return finish(false, QStringLiteral("логин или пароль длиннее 255 байт"));
        QByteArray auth;
        auth.append('\x01');
        auth.append(static_cast<char>(user.size()));
        auth.append(user);
        auth.append(static_cast<char>(pass.size()));
        auth.append(pass);
        m_step = Step::SocksAuth;
        m_socket.write(auth);
        return;
    }
    case Step::SocksAuth:
        if (m_buf.size() < 2)
            return;
        if (m_buf[1] != '\0')
            return finish(false, QStringLiteral("неверный логин или пароль"));
        m_buf.remove(0, 2);
        return sendSocksConnect();
    case Step::SocksConnect:
        if (m_buf.size() < 2)
            return;
        if (static_cast<quint8>(m_buf[0]) != 0x05)
            return finish(false, QStringLiteral("неверный ответ SOCKS5"));
        if (m_buf[1] != '\0')
            return finish(false, socksReply(static_cast<quint8>(m_buf[1])));
        return finish(true, QStringLiteral("SOCKS5 · сервер игры доступен"));
    case Step::HttpStatus: {
        const int end = m_buf.indexOf("\r\n");
        if (end < 0) {
            if (m_buf.size() > kMaxHttpHeader)
                finish(false, QStringLiteral("сервер не HTTP-прокси"));
            return;
        }
        const QList<QByteArray> parts = m_buf.left(end).split(' ');
        if (parts.size() < 2 || !parts[0].startsWith("HTTP/1."))
            return finish(false, QStringLiteral("сервер не HTTP-прокси"));
        const int status = parts[1].toInt();
        if (status == 200)
            return finish(true, QStringLiteral("HTTP · сервер игры доступен"));
        if (status == 407)
            return finish(false, m_proxy.authEnabled ? QStringLiteral("неверный логин или пароль")
                                                     : QStringLiteral("прокси требует логин и пароль"));
        // 400/405 — частый ответ обычного веб-сервера, а не прокси.
        return finish(false, QStringLiteral("CONNECT не принят (HTTP %1) — это не HTTP-прокси или "
                                            "он не пускает к серверу игры").arg(status));
    }
    case Step::Connecting:
    case Step::Done:
        return;
    }
}

void ProxyProbe::finish(bool ok, const QString &message)
{
    if (m_step == Step::Done)
        return; // итог уже отправлен
    m_step = Step::Done;
    m_timeout.stop();
    m_socket.abort();
    emit finished(ok, m_pingMs, message);
    deleteLater();
}
