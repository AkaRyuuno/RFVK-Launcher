#pragma once
// Слот маршрутизации клиента RF Online (прямой IP или прокси).
//
// Каждый слот вмещает не более kMaxClientsPerSlot (5) одновременно
// работающих клиентов игры (ограничение сервера игры на 1 IP).
// Слот «direct» всегда существует и представляет прямое подключение
// через локальный сетевой адаптер игрока; остальные — SOCKS5 или HTTP CONNECT.
// Слот из источника (sourceId не пуст) создаётся, когда клиенту выдан прокси
// из списка по URL, и исчезает, когда закрыто последнее его окно; в
// настройки он не сохраняется и не редактируется.

#include "rfvk_proxy_shared.h"

#include <QJsonObject>
#include <QList>
#include <QString>

constexpr int kMaxClientsPerSlot = 5;

struct ProxySlot {
    using Type = rfvk_proxy::ProxyType;

    QString id;                 // "direct" или "proxy_<uuid>"
    QString name;               // отображаемое имя
    Type type = Type::Direct;
    QString host;               // адрес прокси (IP или домен)
    quint16 port = 0;
    bool authEnabled = false;
    QString username;
    QString password;           // в настройках — только через DPAPI (ProxyManager)
    QList<qint64> activePids;   // PID запущенных через слот клиентов
    int pingMs = -1;            // время TCP-подключения к прокси (-1 — не проверялся)
    bool testOk = true;         // итог последней проверки (false — ошибка)
    QString lastStatus;         // строка состояния для карточки
    bool enabled = true;        // участвует ли в авто-распределении
    QString sourceId;           // источник прокси (пусто — добавлен вручную)
    QString sourceName;

    static QString directId() { return QStringLiteral("direct"); }

    // Тип прокси, который можно сохранить в слот (не Direct).
    static bool isProxyType(int t)
    {
        return t == static_cast<int>(Type::Socks5) || t == static_cast<int>(Type::HttpConnect);
    }

    int activeClients() const { return static_cast<int>(activePids.size()); }
    bool isFull() const { return activeClients() >= kMaxClientsPerSlot; }
    bool isDirect() const { return id == directId(); }
    bool isFromSource() const { return !sourceId.isEmpty(); }

    QString capacityString() const
    {
        return QStringLiteral("%1/%2").arg(activeClients()).arg(kMaxClientsPerSlot);
    }

    QString typeName() const
    {
        switch (type) {
        case Type::Direct: return QStringLiteral("Прямой");
        case Type::Socks5: return QStringLiteral("SOCKS5");
        case Type::HttpConnect: return QStringLiteral("HTTP");
        }
        return QStringLiteral("Неизвестно");
    }

    // Без пароля: его шифрует и хранит ProxyManager.
    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj[QStringLiteral("id")] = id;
        obj[QStringLiteral("name")] = name;
        obj[QStringLiteral("type")] = static_cast<int>(type);
        obj[QStringLiteral("host")] = host;
        obj[QStringLiteral("port")] = static_cast<int>(port);
        obj[QStringLiteral("authEnabled")] = authEnabled;
        obj[QStringLiteral("username")] = username;
        obj[QStringLiteral("enabled")] = enabled;
        return obj;
    }

    // Пустой id — запись битая или неизвестного типа, её нужно пропустить.
    static ProxySlot fromJson(const QJsonObject &obj)
    {
        ProxySlot s;
        const int type = obj.value(QStringLiteral("type")).toInt();
        const int port = obj.value(QStringLiteral("port")).toInt();
        if (!isProxyType(type) || port <= 0 || port > 65535)
            return s;
        s.id = obj.value(QStringLiteral("id")).toString();
        s.name = obj.value(QStringLiteral("name")).toString();
        s.type = static_cast<Type>(type);
        s.host = obj.value(QStringLiteral("host")).toString();
        s.port = static_cast<quint16>(port);
        s.authEnabled = obj.value(QStringLiteral("authEnabled")).toBool(false);
        s.username = obj.value(QStringLiteral("username")).toString();
        s.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
        return s;
    }
};
