#include "AutoLoginStore.h"
#include "VkCrypto.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

namespace {
const QString kFg = QStringLiteral("fgEnc");
const QString kChars = QStringLiteral("characters");
const QString kChosenName = QStringLiteral("charName");
const QString kChosenSlot = QStringLiteral("charSlot");
} // namespace

AutoLoginStore::AutoLoginStore()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_path = QDir(dir).filePath(QStringLiteral("autologin.json"));
    load();
}

QJsonObject AutoLoginStore::entry(const QString &id) const
{
    return m_root.value(id).toObject();
}

void AutoLoginStore::setEntry(const QString &id, const QJsonObject &o)
{
    if (o.isEmpty())
        m_root.remove(id);
    else
        m_root[id] = o;
    save();
}

bool AutoLoginStore::hasFg(const QString &id) const
{
    return !entry(id).value(kFg).toString().isEmpty();
}

QByteArray AutoLoginStore::fg(const QString &id) const
{
    const QString hex = entry(id).value(kFg).toString();
    if (hex.isEmpty())
        return {};
    return VkCrypto::dpapiUnprotect(QByteArray::fromHex(hex.toLatin1()));
}

void AutoLoginStore::setFg(const QString &id, const QString &fg)
{
    QJsonObject o = entry(id);
    if (fg.isEmpty())
        o.remove(kFg);
    else
        o[kFg] = QString::fromLatin1(VkCrypto::dpapiProtect(fg.toLatin1()).toHex());
    setEntry(id, o);
}

QList<SavedCharacter> AutoLoginStore::characters(const QString &id) const
{
    QList<SavedCharacter> out;
    for (const QJsonValue &v : entry(id).value(kChars).toArray()) {
        const QJsonObject c = v.toObject();
        SavedCharacter s;
        s.slot = c.value(QStringLiteral("slot")).toInt(-1);
        s.name = c.value(QStringLiteral("name")).toString();
        s.level = c.value(QStringLiteral("level")).toInt();
        s.race = c.value(QStringLiteral("race")).toInt();
        if (!s.name.isEmpty())
            out << s;
    }
    return out;
}

void AutoLoginStore::setCharacters(const QString &id, const QList<SavedCharacter> &list)
{
    QJsonArray arr;
    for (const SavedCharacter &s : list) {
        QJsonObject c;
        c[QStringLiteral("slot")] = s.slot;
        c[QStringLiteral("name")] = s.name;
        c[QStringLiteral("level")] = s.level;
        c[QStringLiteral("race")] = s.race;
        arr.append(c);
    }
    QJsonObject o = entry(id);
    o[kChars] = arr;
    setEntry(id, o);
}

QString AutoLoginStore::chosenName(const QString &id) const
{
    return entry(id).value(kChosenName).toString();
}

int AutoLoginStore::chosenSlot(const QString &id) const
{
    return entry(id).value(kChosenSlot).toInt(-1);
}

void AutoLoginStore::setChosen(const QString &id, const QString &name, int slot)
{
    QJsonObject o = entry(id);
    if (name.isEmpty()) {
        o.remove(kChosenName);
        o.remove(kChosenSlot);
    } else {
        o[kChosenName] = name;
        o[kChosenSlot] = slot;
    }
    setEntry(id, o);
}

void AutoLoginStore::remove(const QString &id)
{
    if (m_root.contains(id)) {
        m_root.remove(id);
        save();
    }
}

void AutoLoginStore::load()
{
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly))
        m_root = QJsonDocument::fromJson(f.readAll()).object();
}

void AutoLoginStore::save() const
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(m_root).toJson(QJsonDocument::Indented));
}
