#include "AccountModel.h"
#include <QRegularExpression>

AccountModel::AccountModel(QObject *parent) : QAbstractListModel(parent) {}

int AccountModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QString AccountModel::initialsOf(const QString &nick)
{
    const QString s = nick.trimmed();
    if (s.isEmpty())
        return QStringLiteral("?");
    const QStringList parts = s.split(QRegularExpression(QStringLiteral("[\\s_]+")), Qt::SkipEmptyParts);
    if (parts.size() >= 2)
        return (parts.at(0).left(1) + parts.at(1).left(1)).toUpper();
    // Одно слово — берём первые две буквы.
    return s.left(2).toUpper();
}

QVariant AccountModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const Account &a = m_items.at(index.row());
    switch (role) {
    case IdRole: return a.id;
    case LoginRole: return a.login;
    case LoginKindRole: return a.loginKind;
    case NickRole: return a.nick;
    case UidRole: return a.uid;
    case PersIdRole: return a.persId;
    case SessionStateRole: return a.sessionState;
    case SessionExpiryRole: return a.sessionExpiry;
    case SessionRefreshedRole: return a.sessionRefreshed;
    case AvatarRole: return a.avatarPath;
    case InitialsRole: return initialsOf(a.nick.isEmpty() ? a.login : a.nick);
    default: return {};
    }
}

QHash<int, QByteArray> AccountModel::roleNames() const
{
    return {
        {IdRole, "accId"},
        {LoginRole, "login"},
        {LoginKindRole, "loginKind"},
        {NickRole, "nick"},
        {UidRole, "uid"},
        {PersIdRole, "persId"},
        {SessionStateRole, "sessionState"},
        {SessionExpiryRole, "sessionExpiry"},
        {SessionRefreshedRole, "sessionRefreshed"},
        {AvatarRole, "avatarPath"},
        {InitialsRole, "initials"},
    };
}

void AccountModel::reset(const QVector<Account> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
}

int AccountModel::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items.at(i).id == id)
            return i;
    return -1;
}

Account AccountModel::at(int row) const
{
    if (row < 0 || row >= m_items.size())
        return {};
    return m_items.at(row);
}
