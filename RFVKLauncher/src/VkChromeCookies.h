#pragma once
#include <QList>
#include <QString>

// Чтение и расшифровка cookie встроенного Chromium (CEF) GameCenter —
// источник SSO-сессии VK Play для импорта без пароля.
namespace VkChromeCookies {

struct Cookie {
    QString domain;
    QString name;
    QString value;
};

// Профиль CEF GameCenter по умолчанию: %LOCALAPPDATA%\GameCenter\Cache\Chrome.
QString defaultCefDir();

// Расшифровывает cookie для хостов, содержащих один из hostsLike.
// Пусто, если ключ/БД недоступны. err (если задан) получает текст ошибки.
QList<Cookie> decrypt(const QString &cefDir, const QStringList &hostsLike,
                      QString *err = nullptr);

} // namespace VkChromeCookies
