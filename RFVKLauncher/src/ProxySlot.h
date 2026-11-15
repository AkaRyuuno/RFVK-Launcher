#pragma once
// Слот маршрутизации клиента RF Online (прямой IP или прокси).
//
// Каждый слот вмещает не более kMaxClientsPerSlot (5) одновременно
// работающих клиентов игры (ограничение сервера игры на 1 IP).
// Слот 0 ("direct") всегда существует и представляет прямое подключение
// через локальный сетевой адаптер игрока.
// Слоты 1..N представляют SOCKS5 или HTTP CONNECT прокси.

#include <QJsonObject>
#include <QList>
#include <QString>
#include <cstdint>

constexpr int kMaxClientsPerSlot = 5;

struct ProxySlot {
    QString id;              // "direct" или "proxy_<uuid>"
    QString name;            // Отображаемое имя (например "Прямой IP", "Proxy Германия")
    int type = 0;            // 0 = Direct, 1 = SOCKS5, 2 = HTTP
    QString host;            // Адрес прокси (IP или домен)
    quint16 port = 0;        // Порт прокси
    bool authEnabled = false;// Включена ли авторизация по логину/паролю
    QString username;        // Логин
    QString password;        // Пароль
    int maxClients = kMaxClientsPerSlot; // 5 клиентов максимум
    QList<qint64> activePids;// PIDs активных процессов RF_Online.exe
    int pingMs = -1;         // Задержка в мс (-1 = не проверялся)
    QString lastStatus;      // Статусное сообщение ("Доступен", "5/5 занято", etc.)
    bool enabled = true;     // Разрешён ли для авто-распределения

    int activeClients() const { return activePids.size(); }
    bool isFull() const { return activeClients() >= maxClients; }
    bool isDirect() const { return id == QStringLiteral("direct") || type == 0; }

    QString capacityString() const {
        return QString::number(activeClients()) + QStringLiteral("/") + QString::number(maxClients);
    }

    QString typeName() const {
        if (isDirect())
            return QStringLiteral("Прямой");
        if (type == 1)
            return QStringLiteral("SOCKS5");
        if (type == 2)
            return QStringLiteral("HTTP");
        return QStringLiteral("Неизвестно");
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj[QStringLiteral("id")] = id;
        obj[QStringLiteral("name")] = name;
        obj[QStringLiteral("type")] = type;
        obj[QStringLiteral("host")] = host;
        obj[QStringLiteral("port")] = static_cast<int>(port);
        obj[QStringLiteral("authEnabled")] = authEnabled;
        obj[QStringLiteral("username")] = username;
        obj[QStringLiteral("password")] = password;
        obj[QStringLiteral("enabled")] = enabled;
        return obj;
    }

    static ProxySlot fromJson(const QJsonObject &obj) {
        ProxySlot s;
        s.id = obj.value(QStringLiteral("id")).toString();
        s.name = obj.value(QStringLiteral("name")).toString();
        s.type = obj.value(QStringLiteral("type")).toInt(0);
        s.host = obj.value(QStringLiteral("host")).toString();
        s.port = static_cast<quint16>(obj.value(QStringLiteral("port")).toInt(0));
        s.authEnabled = obj.value(QStringLiteral("authEnabled")).toBool(false);
        s.username = obj.value(QStringLiteral("username")).toString();
        s.password = obj.value(QStringLiteral("password")).toString();
        s.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
        return s;
    }
};
