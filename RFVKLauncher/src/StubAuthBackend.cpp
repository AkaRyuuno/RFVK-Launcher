#include "StubAuthBackend.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>

StubAuthBackend::StubAuthBackend(QObject *parent) : IAuthBackend(parent)
{
    // Первый запуск (файла ещё нет) — засеять демо-аккаунтами; иначе уважать
    // сохранённый список, даже если он пуст.
    if (QFile::exists(storePath()))
        load();
    else
        seed();
}

// ── Проигрывание последовательности шагов с задержками ──────────────────────
void StubAuthBackend::runSteps(const QVector<Step> &steps, std::function<void()> onDone)
{
    auto remaining = std::make_shared<QVector<Step>>(steps);
    auto done = std::make_shared<std::function<void()>>(std::move(onDone));
    const int total = steps.size();
    auto idx = std::make_shared<int>(0);
    // Рекурсивный шаг через shared self, чтобы захватить самого себя в лямбде.
    auto self = std::make_shared<std::function<void()>>();
    *self = [this, remaining, done, self, idx, total]() {
        if (remaining->isEmpty()) {
            if (*done)
                (*done)();
            return;
        }
        const Step s = remaining->first();
        remaining->removeFirst();
        emit stepProgress(++(*idx), total);
        emit progress(s.text, s.level);
        QTimer::singleShot(s.delayMs, this, [self]() { (*self)(); });
    };
    (*self)();
}

void StubAuthBackend::setBusy(bool b)
{
    if (m_busy == b)
        return;
    m_busy = b;
    emit busyChanged(b);
}

int StubAuthBackend::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_accounts.size(); ++i)
        if (m_accounts.at(i).id == id)
            return i;
    return -1;
}

// ── Импорт сессии из GameCenter (основной путь, без пароля) ─────────────────
void StubAuthBackend::importSession()
{
    if (m_busy)
        return;
    setBusy(true);
    runSteps(
        {
            {QStringLiteral("Ищу установленный GameCenter…"), 0, 500},
            {QStringLiteral("Читаю сессию из профиля GameCenter…"), 0, 650},
            {QStringLiteral("Проверяю сессию в VK Play…"), 0, 700},
            {QStringLiteral("Получаю профиль игрока…"), 0, 550},
        },
        [this]() {
            // Импорт возвращает текущего мастера GameCenter — демо-аккаунт «Ксарион».
            Account a;
            const int idx = [this]() {
                for (int i = 0; i < m_accounts.size(); ++i)
                    if (m_accounts.at(i).uid == QStringLiteral("100000001"))
                        return i;
                return -1;
            }();
            if (idx >= 0)
                a = m_accounts.at(idx);
            a.uid = QStringLiteral("100000001");
            a.id = QStringLiteral("uid-100000001");
            a.login = QStringLiteral("ks***@mail.ru");
            a.loginKind = QStringLiteral("email");
            a.nick = QStringLiteral("Ксарион");
            a.persId = QStringLiteral("100000001");
            a.sessionState = Session::Active;
            a.sessionExpiry = QDateTime::currentDateTime().addDays(89);
            a.sessionRefreshed = QDateTime::currentDateTime();
            if (idx >= 0)
                m_accounts[idx] = a;
            else
                m_accounts.prepend(a);
            persist();
            emit accountsChanged(m_accounts);
            emit progress(QStringLiteral("Сессия импортирована из GameCenter · ") + a.nick, 1);
            setBusy(false);
            emit finished(true, QStringLiteral("Сессия импортирована из GameCenter · ") + a.nick);
        });
}

// ── Запуск игры: сессия -> код -> старт RF_Online.exe ──────────────────────
void StubAuthBackend::launch(const QString &accountId)
{
    if (m_busy)
        return;
    const int idx = indexOfId(accountId);
    if (idx < 0) {
        emit error(QStringLiteral("Аккаунт не найден"));
        emit finished(false, QStringLiteral("Аккаунт не найден"));
        return;
    }
    const Account a = m_accounts.at(idx);
    setBusy(true);

    // Сессии нет или истекла — запуск невозможен.
    if (a.sessionState == Session::Expired || a.sessionState == Session::None) {
        runSteps({{QStringLiteral("Проверяю сессию · ") + a.nick, 0, 500}}, [this, a]() {
            emit progress(QStringLiteral("Сессия недействительна — нужен импорт из GameCenter"), 2);
            setBusy(false);
            emit error(QStringLiteral("Сессия истекла. Нужен импорт из GameCenter"));
            emit finished(false, QStringLiteral("Сессия истекла · ") + a.nick);
        });
        return;
    }

    // Демо несовпадения: аккаунт по VK ID «Mira_Bellato» сервер отклоняет.
    const bool rejected = (a.loginKind == QStringLiteral("vkid"));

    runSteps(
        {
            {QStringLiteral("Обновляю сессию · ") + a.nick + QStringLiteral("…"), 0, 700},
            {QStringLiteral("Получаю код запуска…"), 0, 750},
            {QStringLiteral("Готовлю запуск клиента…"), 0, 500},
        },
        [this, a, idx, rejected]() {
            if (rejected) {
                emit progress(QStringLiteral("VK Play отклонил сессию «") + a.nick + QStringLiteral("»"), 2);
                // Сессия признана истёкшей.
                m_accounts[idx].sessionState = Session::Expired;
                persist();
                emit accountsChanged(m_accounts);
                setBusy(false);
                emit error(QStringLiteral("VK Play отклонил сессию. Войдите в GameCenter под этим "
                                          "аккаунтом и повторите импорт"));
                emit finished(false, QStringLiteral("VK Play отклонил сессию «") + a.nick + QStringLiteral("»"));
                return;
            }
            // Успех: сессия подтверждена, код получен. Фактический старт
            // клиента RF_Online.exe делает контроллер (у него путь к игре).
            m_accounts[idx].sessionRefreshed = QDateTime::currentDateTime();
            persist();
            emit accountsChanged(m_accounts);
            emit progress(QStringLiteral("Код запуска получен · ") + a.nick, 1);
            setBusy(false);
            // Заглушка не имеет настоящего токена — передаём демонстрационный код.
            const QString code = QStringLiteral("STUBCODE-") + a.persId;
            emit readyToLaunch(a.id, a.persId, code);
        });
}

// ── Обновление сессии ───────────────────────────────────────────────────────
void StubAuthBackend::refreshSession(const QString &accountId)
{
    if (m_busy)
        return;
    const int idx = indexOfId(accountId);
    if (idx < 0) {
        emit error(QStringLiteral("Аккаунт не найден"));
        emit finished(false, QStringLiteral("Аккаунт не найден"));
        return;
    }
    const Account a = m_accounts.at(idx);
    setBusy(true);

    // Демо-ошибка: вход по VK ID без локальной сессии -> 401 при refresh.
    const bool unauthorized = (a.loginKind == QStringLiteral("vkid"));

    runSteps(
        {
            {QStringLiteral("Обновляю сессию · ") + a.nick + QStringLiteral("…"), 0, 700},
            {QStringLiteral("Запрашиваю новый токен…"), 0, 650},
        },
        [this, idx, a, unauthorized]() {
            if (unauthorized) {
                emit progress(QStringLiteral("Обновление сессии: HTTP 401 Unauthorized"), 2);
                m_accounts[idx].sessionState = Session::Expired;
                persist();
                emit accountsChanged(m_accounts);
                setBusy(false);
                emit error(QStringLiteral("Сессия отклонена. Нужен импорт из GameCenter"));
                emit finished(false, QStringLiteral("Сессия отклонена · ") + a.nick);
                return;
            }
            m_accounts[idx].sessionState = Session::Active;
            m_accounts[idx].sessionExpiry = QDateTime::currentDateTime().addSecs(3600);
            m_accounts[idx].sessionRefreshed = QDateTime::currentDateTime();
            persist();
            emit accountsChanged(m_accounts);
            emit progress(QStringLiteral("Сессия обновлена · ") + a.nick, 1);
            setBusy(false);
            emit finished(true, QStringLiteral("Сессия обновлена · ") + a.nick);
        });
}

// ── Удаление аккаунта (с отзывом сессии на сервере) ─────────────────────────
void StubAuthBackend::deleteAccount(const QString &accountId)
{
    if (m_busy)
        return;
    const int idx = indexOfId(accountId);
    if (idx < 0)
        return;
    const Account a = m_accounts.at(idx);
    setBusy(true);
    runSteps(
        {
            {QStringLiteral("Отзываю сессию в VK Play…"), 0, 550},
            {QStringLiteral("Удаляю аккаунт из лаунчера…"), 0, 400},
        },
        [this, idx, a]() {
            m_accounts.removeAt(idx);
            persist();
            emit accountsChanged(m_accounts);
            emit progress(QStringLiteral("Аккаунт «") + a.nick + QStringLiteral("» удалён из лаунчера"), 1);
            setBusy(false);
            emit finished(true, QStringLiteral("Аккаунт «") + a.nick + QStringLiteral("» удалён"));
        });
}

// ── Хранение списка аккаунтов (JSON рядом с данными приложения) ──────────────
QString StubAuthBackend::storePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("accounts.json"));
}

void StubAuthBackend::persist() const
{
    QJsonArray arr;
    for (const Account &a : m_accounts) {
        QJsonObject o;
        o[QStringLiteral("id")] = a.id;
        o[QStringLiteral("login")] = a.login;
        o[QStringLiteral("loginKind")] = a.loginKind;
        o[QStringLiteral("nick")] = a.nick;
        o[QStringLiteral("uid")] = a.uid;
        o[QStringLiteral("persId")] = a.persId;
        o[QStringLiteral("sessionState")] = a.sessionState;
        o[QStringLiteral("sessionExpiry")] = a.sessionExpiry.toString(Qt::ISODate);
        o[QStringLiteral("sessionRefreshed")] = a.sessionRefreshed.toString(Qt::ISODate);
        o[QStringLiteral("avatarPath")] = a.avatarPath;
        arr.append(o);
    }
    const QString path = storePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void StubAuthBackend::load()
{
    QFile f(storePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return;
    m_accounts.clear();
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        Account a;
        a.id = o.value(QStringLiteral("id")).toString();
        a.login = o.value(QStringLiteral("login")).toString();
        a.loginKind = o.value(QStringLiteral("loginKind")).toString();
        a.nick = o.value(QStringLiteral("nick")).toString();
        a.uid = o.value(QStringLiteral("uid")).toString();
        a.persId = o.value(QStringLiteral("persId")).toString();
        a.sessionState = o.value(QStringLiteral("sessionState")).toInt();
        a.sessionExpiry = QDateTime::fromString(o.value(QStringLiteral("sessionExpiry")).toString(), Qt::ISODate);
        a.sessionRefreshed =
            QDateTime::fromString(o.value(QStringLiteral("sessionRefreshed")).toString(), Qt::ISODate);
        a.avatarPath = o.value(QStringLiteral("avatarPath")).toString();
        m_accounts.append(a);
    }
}

// ── Три тестовых аккаунта: Активна / Скоро истечёт / Истекла ────────────────
void StubAuthBackend::seed()
{
    const QDateTime now = QDateTime::currentDateTime();
    Account a;
    a.id = QStringLiteral("uid-100000001");
    a.login = QStringLiteral("ks***@mail.ru");
    a.loginKind = QStringLiteral("email");
    a.nick = QStringLiteral("Ксарион");
    a.uid = QStringLiteral("100000001");
    a.persId = QStringLiteral("100000001");
    a.sessionState = Session::Active;
    a.sessionExpiry = now.addDays(2);
    a.sessionRefreshed = now.addSecs(-720);
    m_accounts.append(a);

    Account b;
    b.id = QStringLiteral("uid-100000002");
    b.login = QStringLiteral("10•••02");
    b.loginKind = QStringLiteral("vkid");
    b.nick = QStringLiteral("Mira_Bellato");
    b.uid = QStringLiteral("100000002");
    b.persId = QStringLiteral("100000002");
    b.sessionState = Session::Expiring;
    b.sessionExpiry = now.addSecs(2 * 3600);
    b.sessionRefreshed = now.addSecs(-3600);
    m_accounts.append(b);

    Account c;
    c.id = QStringLiteral("phone-79990000000");
    c.login = QStringLiteral("+7 999 ***-00-00");
    c.loginKind = QStringLiteral("phone");
    c.nick = QStringLiteral("Nemesis");
    c.uid = QStringLiteral("204418833");
    c.persId = QStringLiteral("204418833");
    c.sessionState = Session::Expired;
    c.sessionExpiry = now.addDays(-1);
    c.sessionRefreshed = now.addDays(-5);
    m_accounts.append(c);

    Account d;
    d.id = QStringLiteral("uid-204418999");
    d.login = QStringLiteral("40•••••");
    d.loginKind = QStringLiteral("vkid");
    d.nick = QStringLiteral("Accretia_Main");
    d.uid = QStringLiteral("204418999");
    d.persId = QStringLiteral("204418999");
    d.sessionState = Session::None;
    m_accounts.append(d);

    persist();
}
