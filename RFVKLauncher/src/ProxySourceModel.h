#pragma once
#include "ProxySource.h"

#include <QAbstractListModel>
#include <QSet>
#include <QTimer>
#include <QVector>

// Источники прокси для QML. Во время массовой проверки строки меняются
// сотни раз в секунду — изменения копятся и уходят в интерфейс раз в
// kFlushMs (markDirty), а не по каждой записи.
class ProxySourceModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        UrlRole,
        DefaultTypeRole,
        EnabledRole,
        TotalRole,
        OkRole,
        BadRole,
        UncheckedRole,
        SkippedRole,
        InUseClientsRole,
        InUseProxiesRole,
        FetchingRole,
        CheckingRole,
        CheckDoneRole,
        CheckTotalRole,
        StatusRole,
        StatusErrorRole,
        FetchedAtRole
    };

    explicit ProxySourceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int indexOfId(const QString &id) const;
    const ProxySource &at(int row) const { return m_items.at(row); }
    // Изменить на месте; после — touch() или markDirty().
    ProxySource &ref(int row) { return m_items[row]; }
    const QVector<ProxySource> &items() const { return m_items; }

    void add(const ProxySource &s);
    void remove(int row);
    void touch(int row);
    void markDirty(int row);

private:
    void flush();

    QVector<ProxySource> m_items;
    QSet<QString> m_dirty;  // id: строки могут сдвинуться до flush()
    QTimer m_flushTimer;
};
