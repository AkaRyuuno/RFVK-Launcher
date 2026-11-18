#include "ProxySourceModel.h"

#include <QDateTime>

#include <utility>

namespace {
constexpr int kFlushMs = 300;
}

ProxySourceModel::ProxySourceModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(kFlushMs);
    connect(&m_flushTimer, &QTimer::timeout, this, &ProxySourceModel::flush);
}

int ProxySourceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant ProxySourceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const ProxySource &s = m_items.at(index.row());
    switch (role) {
    case IdRole: return s.id;
    case NameRole: return s.name;
    case UrlRole: return s.url;
    case DefaultTypeRole: return static_cast<int>(s.defaultType);
    case EnabledRole: return s.enabled;
    case TotalRole: return static_cast<int>(s.entries.size());
    case OkRole: return s.okCount;
    case BadRole: return s.badCount;
    case UncheckedRole: return static_cast<int>(s.entries.size()) - s.okCount - s.badCount;
    case SkippedRole: return s.skipped;
    case InUseClientsRole: return s.inUseClients;
    case InUseProxiesRole: return s.inUseProxies;
    case FetchingRole: return s.fetching;
    case CheckingRole: return s.checking();
    case CheckDoneRole: return s.checkDone;
    case CheckTotalRole: return s.checkTotal;
    case StatusRole: return s.status;
    case StatusErrorRole: return s.statusError;
    case FetchedAtRole:
        return s.fetchedAt > 0
            ? QDateTime::fromSecsSinceEpoch(s.fetchedAt).toString(QStringLiteral("dd.MM.yyyy HH:mm"))
            : QString();
    default: return {};
    }
}

QHash<int, QByteArray> ProxySourceModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {IdRole, "sourceId"},
        {NameRole, "name"},
        {UrlRole, "url"},
        {DefaultTypeRole, "defaultType"},
        {EnabledRole, "enabled"},
        {TotalRole, "total"},
        {OkRole, "okCount"},
        {BadRole, "badCount"},
        {UncheckedRole, "uncheckedCount"},
        {SkippedRole, "skipped"},
        {InUseClientsRole, "inUseClients"},
        {InUseProxiesRole, "inUseProxies"},
        {FetchingRole, "fetching"},
        {CheckingRole, "checking"},
        {CheckDoneRole, "checkDone"},
        {CheckTotalRole, "checkTotal"},
        {StatusRole, "status"},
        {StatusErrorRole, "statusError"},
        {FetchedAtRole, "fetchedAt"}
    };
    return roles;
}

int ProxySourceModel::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id)
            return i;
    }
    return -1;
}

void ProxySourceModel::add(const ProxySource &s)
{
    const int row = static_cast<int>(m_items.size());
    beginInsertRows(QModelIndex(), row, row);
    m_items.append(s);
    endInsertRows();
}

void ProxySourceModel::remove(int row)
{
    if (row < 0 || row >= m_items.size())
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_dirty.remove(m_items.at(row).id);
    m_items.removeAt(row);
    endRemoveRows();
}

void ProxySourceModel::touch(int row)
{
    if (row < 0 || row >= m_items.size())
        return;
    m_dirty.remove(m_items.at(row).id);
    emit dataChanged(index(row), index(row));
}

void ProxySourceModel::markDirty(int row)
{
    if (row < 0 || row >= m_items.size())
        return;
    m_dirty.insert(m_items.at(row).id);
    if (!m_flushTimer.isActive())
        m_flushTimer.start();
}

void ProxySourceModel::flush()
{
    const QSet<QString> ids = std::exchange(m_dirty, {});
    for (const QString &id : ids) {
        const int row = indexOfId(id);
        if (row >= 0)
            emit dataChanged(index(row), index(row));
    }
}
