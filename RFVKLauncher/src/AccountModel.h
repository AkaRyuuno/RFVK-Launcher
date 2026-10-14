#pragma once
#include "Account.h"
#include <QAbstractListModel>
#include <QVector>

// Модель списка аккаунтов для QML (ListView в сайдбаре).
// Источник истины о сессиях — бэкенд; контроллер зеркалит его данные сюда
// через reset()/upsert()/remove(), сохраняя выбранный аккаунт по id.
class AccountModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        LoginRole,
        LoginKindRole,
        NickRole,
        UidRole,
        PersIdRole,
        SessionStateRole,
        SessionExpiryRole,
        SessionRefreshedRole,
        AvatarRole,
        InitialsRole
    };

    explicit AccountModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Полная замена списка (приход accountsChanged от бэкенда).
    void reset(const QVector<Account> &items);
    int indexOfId(const QString &id) const;
    Account at(int row) const;
    // Инициалы для аватара-заглушки: по двум словам ника или первые две буквы.
    static QString initialsOf(const QString &nick);
    const QVector<Account> &items() const { return m_items; }

private:
    QVector<Account> m_items;
};
