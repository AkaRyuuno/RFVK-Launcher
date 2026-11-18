#pragma once
#include "ProxySlot.h"
#include <QAbstractListModel>
#include <QVector>

// QAbstractListModel для отображения слотов (Direct + Proxies) в QML.
class ProxyModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        TypeNameRole,
        HostRole,
        PortRole,
        AuthEnabledRole,
        UsernameRole,
        ActiveClientsRole,
        MaxClientsRole,
        CapacityStringRole,
        IsFullRole,
        PingMsRole,
        TestOkRole,
        LastStatusRole,
        EnabledRole,
        IsDirectRole,
        ActivePidsRole,
        SourceIdRole,
        SourceNameRole
    };

    explicit ProxyModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int indexOfId(const QString &id) const;
    const ProxySlot &at(int row) const { return m_slots.at(row); }
    const QVector<ProxySlot> &slotItems() const { return m_slots; }

    void setSlots(const QVector<ProxySlot> &items);
    void addSlot(const ProxySlot &slot);
    void updateSlot(int row, const ProxySlot &slot);
    void removeSlot(int row);

private:
    QVector<ProxySlot> m_slots;
};
