#include "ProxySource.h"

#include <QList>
#include <QSet>
#include <QUrl>

namespace {
// Хост из списка: IPv4 или имя. IPv6 rfvk_proxy.dll не поддерживает.
bool validHost(const QString &h)
{
    if (h.isEmpty() || h.size() >= rfvk_proxy::kHostLen)
        return false;
    for (const QChar c : h) {
        if (!(c.isLetterOrNumber() && c.unicode() < 128) && c != QLatin1Char('.') && c != QLatin1Char('-'))
            return false;
    }
    return true;
}

bool validAuth(const QString &s)
{
    return s.toUtf8().size() < rfvk_proxy::kAuthLen;
}
} // namespace

SourceParseResult parseSourceList(const QByteArray &text, ProxySlot::Type defaultType, int maxEntries)
{
    SourceParseResult r;
    QSet<QString> seen;
    const QList<QByteArray> lines = text.split('\n');
    for (const QByteArray &raw : lines) {
        QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1String("//")))
            continue;

        SourceEntry e;
        e.type = defaultType;
        const int schemeEnd = line.indexOf(QLatin1String("://"));
        const bool hasScheme = schemeEnd > 0;
        if (hasScheme) {
            const QString scheme = line.left(schemeEnd).toLower();
            if (scheme == QLatin1String("http") || scheme == QLatin1String("https"))
                e.type = ProxySlot::Type::HttpConnect;
            else if (scheme == QLatin1String("socks5") || scheme == QLatin1String("socks5h"))
                e.type = ProxySlot::Type::Socks5;
            else {
                ++r.skipped; // socks4 и прочее
                continue;
            }
            line = line.mid(schemeEnd + 3);
        }
        // Хвост вида «/», «#страна», « комментарий».
        for (int i = 0; i < line.size(); ++i) {
            const QChar c = line.at(i);
            if (c == QLatin1Char('/') || c == QLatin1Char('#') || c.isSpace()) {
                line.truncate(i);
                break;
            }
        }

        QString hostPort = line;
        const int at = line.lastIndexOf(QLatin1Char('@'));
        if (at >= 0) {
            const QString userInfo = line.left(at);
            hostPort = line.mid(at + 1);
            const int colon = userInfo.indexOf(QLatin1Char(':'));
            e.username = QUrl::fromPercentEncoding(userInfo.left(colon).toUtf8());
            e.password = colon >= 0 ? QUrl::fromPercentEncoding(userInfo.mid(colon + 1).toUtf8()) : QString();
        } else if (!hasScheme && line.count(QLatin1Char(':')) == 3) {
            const QStringList parts = line.split(QLatin1Char(':'));
            hostPort = parts[0] + QLatin1Char(':') + parts[1];
            e.username = parts[2];
            e.password = parts[3];
        }

        const int colon = hostPort.lastIndexOf(QLatin1Char(':'));
        bool portOk = false;
        const int port = colon > 0 ? hostPort.mid(colon + 1).toInt(&portOk) : 0;
        e.host = hostPort.left(colon);
        if (!portOk || port <= 0 || port > 65535 || !validHost(e.host) || !validAuth(e.username)
            || !validAuth(e.password)) {
            ++r.skipped;
            continue;
        }
        e.port = static_cast<quint16>(port);
        if (seen.contains(e.key()))
            continue;
        seen.insert(e.key());
        r.entries.append(e);
        if (r.entries.size() >= maxEntries)
            break;
    }
    return r;
}
