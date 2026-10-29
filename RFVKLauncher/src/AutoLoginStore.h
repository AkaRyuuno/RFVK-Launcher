#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

// Персонаж аккаунта, как его показал клиент на экране выбора.
struct SavedCharacter {
    int slot = -1;
    QString name;
    int level = 0;
    int race = 0; // раса × 2 + пол, как у клиента
};

// Настройки автовхода по аккаунтам (ключ — id аккаунта лаунчера):
// FG-пароль (под DPAPI, в открытом виде не хранится и в QML не попадает),
// список персонажей с последнего входа и выбранный для автовхода персонаж.
// Хранится отдельно от аккаунтов бэкенда — AppDataLocation\autologin.json.
class AutoLoginStore {
public:
    AutoLoginStore();

    bool hasFg(const QString &id) const;
    QByteArray fg(const QString &id) const; // расшифрованный, ASCII
    void setFg(const QString &id, const QString &fg); // пусто — удалить

    QList<SavedCharacter> characters(const QString &id) const;
    void setCharacters(const QString &id, const QList<SavedCharacter> &list);

    // Выбранный персонаж: имя (главное) и слот (запасной признак). Пусто — не выбирать.
    QString chosenName(const QString &id) const;
    int chosenSlot(const QString &id) const;
    void setChosen(const QString &id, const QString &name, int slot);

    void remove(const QString &id);

private:
    QJsonObject entry(const QString &id) const;
    void setEntry(const QString &id, const QJsonObject &o);
    void load();
    void save() const;

    QString m_path;
    QJsonObject m_root;
};
