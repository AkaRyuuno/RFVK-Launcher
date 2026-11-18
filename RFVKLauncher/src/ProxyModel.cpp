#include "ProxyModel.h"

ProxyModel::ProxyModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ProxyModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_slots.size());
}

QVariant ProxyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_slots.size())
        return {};

    const ProxySlot &s = m_slots.at(index.row());
    switch (role) {
    case IdRole:
        return s.id;
    case NameRole:
        return s.name;
    case TypeRole:
        return static_cast<int>(s.type);
    case TypeNameRole:
        return s.typeName();
    case HostRole:
        return s.host;
    case PortRole:
        return static_cast<int>(s.port);
    case AuthEnabledRole:
        return s.authEnabled;
    case UsernameRole:
        return s.username;
    case ActiveClientsRole:
        return s.activeClients();
    case MaxClientsRole:
        return kMaxClientsPerSlot;
    case CapacityStringRole:
        return s.capacityString();
    case IsFullRole:
        return s.isFull();
    case PingMsRole:
        return s.pingMs;
    case TestOkRole:
        return s.testOk;
    case LastStatusRole:
        return s.lastStatus;
    case EnabledRole:
        return s.enabled;
    case IsDirectRole:
        return s.isDirect();
    case ActivePidsRole: {
        QVariantList list;
        for (qint64 pid : s.activePids)
            list.append(pid);
        return list;
    }
    case SourceIdRole:
        return s.sourceId;
    case SourceNameRole:
        return s.sourceName;
    default:
        return {};
    }
}

QHash<int, QByteArray> ProxyModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {IdRole, "slotId"},
        {NameRole, "name"},
        {TypeRole, "type"},
        {TypeNameRole, "typeName"},
        {HostRole, "host"},
        {PortRole, "port"},
        {AuthEnabledRole, "authEnabled"},
        {UsernameRole, "username"},
        {ActiveClientsRole, "activeClients"},
        {MaxClientsRole, "maxClients"},
        {CapacityStringRole, "capacityString"},
        {IsFullRole, "isFull"},
        {PingMsRole, "pingMs"},
        {TestOkRole, "testOk"},
        {LastStatusRole, "lastStatus"},
        {EnabledRole, "enabled"},
        {IsDirectRole, "isDirect"},
        {ActivePidsRole, "activePids"},
        {SourceIdRole, "sourceId"},
        {SourceNameRole, "sourceName"}
    };
    return roles;
}

int ProxyModel::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots.at(i).id == id)
            return i;
    }
    return -1;
}

void ProxyModel::setSlots(const QVector<ProxySlot> &items)
{
    beginResetModel();
    m_slots = items;
    endResetModel();
}

void ProxyModel::addSlot(const ProxySlot &slot)
{
    const int row = static_cast<int>(m_slots.size());
    beginInsertRows(QModelIndex(), row, row);
    m_slots.append(slot);
    endInsertRows();
}

void ProxyModel::updateSlot(int row, const ProxySlot &slot)
{
    if (row < 0 || row >= m_slots.size())
        return;
    m_slots[row] = slot;
    emit dataChanged(index(row), index(row));
}

void ProxyModel::removeSlot(int row)
{
    if (row < 0 || row >= m_slots.size())
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_slots.removeAt(row);
    endRemoveRows();
}
