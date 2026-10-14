#include "VkPlayAuthBackend.h"
#include "VkChromeCookies.h"
#include "VkCrypto.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrlQuery>

// ─────────────────────────── константы протокола ───────────────────────────
namespace {
const QString TOKEN_URL = QStringLiteral("https://o2-ac.vkplay.ru/token");
const QString TOKEN_EXCHANGE_URL =
    QStringLiteral("https://o2-ext-ac.vkplay.ru/api/v3/pub/oauth2/token");
const QString DROP_URL = QStringLiteral("https://o2-ext-ac.vkplay.ru/api/v3/pub/oauth2/drop");
const QString CLIENT_ID = QStringLiteral("gc.my.com");
const QString GEM_URL = QStringLiteral("https://authdl.vkplay.ru/gem.php");
const QByteArray GEM_UA = QByteArrayLiteral("Downloader/19160"); // иной UA -> gem.php отдаёт 400
const QString PROJECT_ID = QStringLiteral("2045077");            // RF Online
const QString CHANNEL_ID = QStringLiteral("58");
const QString API_URL = QStringLiteral("https://api.vkplay.ru");

QString inferLoginKind(const QString &login)
{
    if (login.contains('@'))
        return QStringLiteral("email");
    QString digits;
    for (QChar c : login)
        if (c.isDigit())
            digits += c;
    const bool phoneish = login.startsWith('+')
                          || (digits.size() >= 10
                              && QRegularExpression(QStringLiteral("^[+()\\-0-9 ]+$")).match(login).hasMatch());
    return phoneish ? QStringLiteral("phone") : QStringLiteral("vkid");
}
} // namespace

// ───────────────────────────── хранилище токенов ───────────────────────────
// JSON рядом с данными приложения; refresh-токен — в DPAPI (hex).
class VkStore {
public:
    VkStore() : m_path(filePath()) { load(); }

    static QString filePath()
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        return QDir(dir).filePath(QStringLiteral("accounts_vk.json"));
    }

    QVector<Account> list() const
    {
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        QVector<Account> out;
        for (const QJsonValue &v : m_arr) {
            const QJsonObject o = v.toObject();
            Account a;
            a.login = o.value(QStringLiteral("login")).toString();
            a.id = a.login;
            a.loginKind = o.value(QStringLiteral("loginKind")).toString();
            if (a.loginKind.isEmpty())
                a.loginKind = inferLoginKind(a.login);
            a.nick = o.value(QStringLiteral("nick")).toString();
            a.uid = o.value(QStringLiteral("uid")).toString();
            a.persId = o.value(QStringLiteral("persId")).toString();
            const bool hasRefresh = !o.value(QStringLiteral("refreshEnc")).toString().isEmpty();
            const qint64 exp = static_cast<qint64>(o.value(QStringLiteral("refreshExpires")).toDouble());
            if (!hasRefresh)
                a.sessionState = Session::None;
            else if (exp && exp <= now)
                a.sessionState = Session::Expired;
            else if (exp && exp - now < 7 * 24 * 3600)
                a.sessionState = Session::Expiring;
            else
                a.sessionState = Session::Active;
            if (exp)
                a.sessionExpiry = QDateTime::fromSecsSinceEpoch(exp);
            out.append(a);
        }
        return out;
    }

    int indexOf(const QString &login) const
    {
        for (int i = 0; i < m_arr.size(); ++i)
            if (m_arr.at(i).toObject().value(QStringLiteral("login")).toString().compare(
                    login, Qt::CaseInsensitive)
                == 0)
                return i;
        return -1;
    }

    QJsonObject obj(const QString &login) const
    {
        const int i = indexOf(login);
        return i >= 0 ? m_arr.at(i).toObject() : QJsonObject{};
    }

    void setFields(const QString &login, const QJsonObject &fields)
    {
        int i = indexOf(login);
        QJsonObject o = i >= 0 ? m_arr.at(i).toObject() : QJsonObject{};
        o[QStringLiteral("login")] = login;
        if (!o.contains(QStringLiteral("loginKind")))
            o[QStringLiteral("loginKind")] = inferLoginKind(login);
        for (auto it = fields.begin(); it != fields.end(); ++it)
            o[it.key()] = it.value();
        if (i >= 0)
            m_arr[i] = o;
        else
            m_arr.append(o);
        save();
    }

    void remove(const QString &login)
    {
        const int i = indexOf(login);
        if (i >= 0) {
            m_arr.removeAt(i);
            save();
        }
    }

    QString getRefresh(const QString &login) const
    {
        const QString hex = obj(login).value(QStringLiteral("refreshEnc")).toString();
        if (hex.isEmpty())
            return {};
        return QString::fromLatin1(VkCrypto::dpapiUnprotect(QByteArray::fromHex(hex.toLatin1())));
    }

    void setRefresh(const QString &login, const QString &token, qint64 expiry)
    {
        QJsonObject f;
        f[QStringLiteral("refreshEnc")] =
            token.isEmpty() ? QString()
                            : QString::fromLatin1(VkCrypto::dpapiProtect(token.toLatin1()).toHex());
        if (expiry)
            f[QStringLiteral("refreshExpires")] = static_cast<double>(expiry);
        setFields(login, f);
    }

    // Привязка импортированной сессии к мастеру по uid (как GameCenter).
    QString findByUid(const QString &uid) const
    {
        if (uid.isEmpty())
            return {};
        for (const QJsonValue &v : m_arr) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("uid")).toString() == uid)
                return o.value(QStringLiteral("login")).toString();
        }
        return {};
    }

private:
    void load()
    {
        QFile f(m_path);
        if (f.open(QIODevice::ReadOnly))
            m_arr = QJsonDocument::fromJson(f.readAll()).array();
    }
    void save()
    {
        QDir().mkpath(QFileInfo(m_path).absolutePath());
        QFile f(m_path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(QJsonDocument(m_arr).toJson(QJsonDocument::Indented));
    }
    QString m_path;
    QJsonArray m_arr;
};

// ───────────────────────────── рабочий объект ───────────────────────────────
// Живёт в рабочем потоке; сетевые запросы — синхронные (QEventLoop) в нём же.
class VkWorker : public QObject {
    Q_OBJECT
public:
    VkWorker() = default;

public slots:
    void doImport()
    {
        emit busyChanged(true);
        QString err;
        QString login, nick, uid;
        qint64 exp = 0;
        QString refresh;
        if (mintViaCookies(&refresh, &exp, &nick, &uid, &err)) {
            login = m_store.findByUid(uid);
            if (login.isEmpty())
                login = uid.isEmpty() ? QStringLiteral("account") : (QStringLiteral("uid-") + uid);
            m_store.setRefresh(login, refresh, exp);
            QJsonObject f;
            f[QStringLiteral("nick")] = nick;
            f[QStringLiteral("uid")] = uid;
            m_store.setFields(login, f);
            emit progress(QStringLiteral("Сессия импортирована из GameCenter · ") + nick, 1);
            emit accountsChanged(m_store.list());
            emit finished(true, QStringLiteral("Сессия импортирована из GameCenter · ") + nick);
            emit busyChanged(false);
        } else {
            fail(err);
        }
    }

    void doRefresh(const QString &login)
    {
        emit busyChanged(true);
        QString err;
        emit progress(QStringLiteral("Обновляю сессию · ") + displayName(login) + QStringLiteral("…"), 0);
        QString at = refreshAccess(login, &err);
        if (at.isEmpty()) {
            m_store.setFields(login, {{QStringLiteral("refreshEnc"), QString()}});
            fail(err.isEmpty() ? QStringLiteral("Сессия отклонена. Нужен импорт из GameCenter") : err);
            return;
        }
        refreshProfile(login, at);
        emit progress(QStringLiteral("Сессия обновлена · ") + displayName(login), 1);
        emit accountsChanged(m_store.list());
        emit finished(true, QStringLiteral("Сессия обновлена · ") + displayName(login));
        emit busyChanged(false);
    }

    void doLaunch(const QString &login)
    {
        emit busyChanged(true);
        QString err;
        // 1) обновить сессию существующим refresh.
        emit progress(QStringLiteral("Обновляю сессию · ") + displayName(login) + QStringLiteral("…"), 0);
        QString at = refreshAccess(login, &err);
        // 2) фолбэк без пароля: взять сессию из GameCenter, если это тот же мастер.
        if (at.isEmpty()) {
            emit progress(QStringLiteral("Беру сессию из GameCenter (без пароля)…"), 0);
            QString r2, nick2, uid2;
            qint64 exp2 = 0;
            QString e2;
            if (mintViaCookies(&r2, &exp2, &nick2, &uid2, &e2)) {
                const QString curUid = m_store.obj(login).value(QStringLiteral("uid")).toString();
                if (curUid.isEmpty() || curUid == uid2) {
                    m_store.setRefresh(login, r2, exp2);
                    at = refreshAccess(login, &err);
                } else {
                    err = QStringLiteral("В GameCenter другой аккаунт (uid ") + uid2
                          + QStringLiteral("). Войдите под нужным и повторите");
                }
            } else {
                err = e2;
            }
        }
        if (at.isEmpty()) {
            fail(err.isEmpty() ? QStringLiteral("Не удалось получить сессию") : err);
            return;
        }
        refreshProfile(login, at);
        // 3) код запуска.
        emit progress(QStringLiteral("Получаю код запуска…"), 0);
        QString code, pers;
        if (!gemLogin(at, &code, &pers, &err)) {
            fail(err);
            return;
        }
        m_store.setFields(login, {{QStringLiteral("persId"), pers}});
        emit progress(QStringLiteral("Код запуска получен · ") + displayName(login), 1);
        emit accountsChanged(m_store.list());
        // Старт RF_Online.exe делает контроллер (у него путь к игре).
        emit readyToLaunch(login, pers, code);
        emit busyChanged(false);
    }

    void doDelete(const QString &login)
    {
        emit busyChanged(true);
        const QString name = displayName(login);
        const QString refresh = m_store.getRefresh(login);
        if (!refresh.isEmpty()) {
            emit progress(QStringLiteral("Отзываю сессию в VK Play…"), 0);
            drop(refresh); // best-effort
        }
        m_store.remove(login);
        emit progress(QStringLiteral("Аккаунт «") + name + QStringLiteral("» удалён из лаунчера"), 1);
        emit accountsChanged(m_store.list());
        emit finished(true, QStringLiteral("Аккаунт «") + name + QStringLiteral("» удалён"));
        emit busyChanged(false);
    }

signals:
    void progress(const QString &step, int level);
    void finished(bool ok, const QString &message);
    void accountsChanged(const QVector<Account> &items);
    void busyChanged(bool busy);
    void error(const QString &humanReadable);
    void readyToLaunch(const QString &accountId, const QString &persId, const QString &code);

private:
    void fail(const QString &msg)
    {
        emit progress(msg, 2);
        emit error(msg);
        emit accountsChanged(m_store.list());
        emit finished(false, msg);
        emit busyChanged(false);
    }

    QString displayName(const QString &login) const
    {
        const QString nick = m_store.obj(login).value(QStringLiteral("nick")).toString();
        return nick.isEmpty() ? login : nick;
    }

    QNetworkAccessManager *nam()
    {
        if (!m_nam)
            m_nam = new QNetworkAccessManager(this);
        return m_nam;
    }

    // Синхронный HTTP в рабочем потоке. post==nullptr -> GET.
    QByteArray http(QNetworkRequest req, const QByteArray *post, int *status, bool *ok)
    {
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = post ? nam()->post(req, *post) : nam()->get(req);
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        if (status)
            *status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (ok)
            *ok = (reply->error() == QNetworkReply::NoError);
        const QByteArray data = reply->readAll();
        reply->deleteLater();
        return data;
    }

    static QString xmlAttr(const QString &xml, const QString &name)
    {
        const QRegularExpressionMatch m =
            QRegularExpression(QStringLiteral("\\b%1=\"([^\"]*)\"").arg(name)).match(xml);
        return m.hasMatch() ? m.captured(1) : QString();
    }

    // refresh_token -> access_token (+ персист ротированного refresh/срока).
    QString refreshAccess(const QString &login, QString *err)
    {
        const QString refresh = m_store.getRefresh(login);
        if (refresh.isEmpty()) {
            if (err)
                *err = QStringLiteral("Нет сохранённой сессии — нужен импорт из GameCenter");
            return {};
        }
        QNetworkRequest r{QUrl(TOKEN_URL)};
        r.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/x-www-form-urlencoded"));
        QUrlQuery form;
        form.addQueryItem(QStringLiteral("client_id"), CLIENT_ID);
        form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
        form.addQueryItem(QStringLiteral("refresh_token"), refresh);
        const QByteArray body = form.toString(QUrl::FullyEncoded).toUtf8();
        int st = 0;
        bool ok = false;
        const QJsonObject j = QJsonDocument::fromJson(http(r, &body, &st, &ok)).object();
        if (!j.contains(QStringLiteral("access_token"))) {
            if (err)
                *err = QStringLiteral("Сессия истекла (HTTP %1)").arg(st);
            return {};
        }
        // Персист ротации, если сервер прислал новый refresh/срок.
        const QString newRefresh = j.value(QStringLiteral("refresh_token")).toString();
        const qint64 exp = j.contains(QStringLiteral("refresh_token_expires_in"))
                               ? QDateTime::currentSecsSinceEpoch()
                                     + static_cast<qint64>(j.value(QStringLiteral("refresh_token_expires_in")).toDouble())
                               : 0;
        if ((!newRefresh.isEmpty() && newRefresh != refresh) || exp)
            m_store.setRefresh(login, newRefresh.isEmpty() ? refresh : newRefresh, exp);
        return j.value(QStringLiteral("access_token")).toString();
    }

    QString gemHint(const QString &hint, const QString &xmlBody)
    {
        QNetworkRequest r{QUrl(GEM_URL + QStringLiteral("?hint=") + hint)};
        r.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/xml"));
        r.setHeader(QNetworkRequest::UserAgentHeader, GEM_UA);
        const QByteArray body = xmlBody.toUtf8();
        int st = 0;
        bool ok = false;
        return QString::fromUtf8(http(r, &body, &st, &ok));
    }

    // hint=Auth -> профиль (ник/uid).
    bool gemProfile(const QString &at, QString *nick, QString *uid)
    {
        const QString body =
            QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                           "<Auth SessionKey=\"%1\" ChannelId=\"%2\" GcLang=\"ru\" UserId=\"\" "
                           "UserId2=\"\" DeviceId=\"\" StatPer=\"0\"/>")
                .arg(at, CHANNEL_ID);
        const QString txt = gemHint(QStringLiteral("Auth"), body);
        const QString n = xmlAttr(txt, QStringLiteral("NickName"));
        const QString u = xmlAttr(txt, QStringLiteral("Uid"));
        if (nick)
            *nick = n.isEmpty() ? xmlAttr(txt, QStringLiteral("Username")) : n;
        if (uid)
            *uid = u;
        return !u.isEmpty() || !n.isEmpty();
    }

    void refreshProfile(const QString &login, const QString &at)
    {
        QString nick, uid;
        if (gemProfile(at, &nick, &uid)) {
            QJsonObject f;
            if (!nick.isEmpty())
                f[QStringLiteral("nick")] = nick;
            if (!uid.isEmpty())
                f[QStringLiteral("uid")] = uid;
            if (!f.isEmpty())
                m_store.setFields(login, f);
        }
    }

    // hint=Login(ProjectId) -> Code(48hex) + PersId.
    bool gemLogin(const QString &at, QString *code, QString *pers, QString *err)
    {
        const QString body =
            QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                           "<Login SessionKey=\"%1\" UserId=\"\" UserId2=\"\" DeviceId=\"\" TypeId=\"0\" "
                           "ProjectId=\"%2\" ShardId=\"0\" Dry=\"0\" GcLang=\"ru\" TermsAware=\"1\" "
                           "FirstLink=\"\" TmrLvid=\"\" RegionData=\"\"/>")
                .arg(at, PROJECT_ID);
        const QString txt = gemHint(QStringLiteral("Login"), body);
        const QRegularExpressionMatch mc =
            QRegularExpression(QStringLiteral("Code=\"([0-9a-fA-F]{48})\"")).match(txt);
        const QRegularExpressionMatch mp =
            QRegularExpression(QStringLiteral("PersId=\"(\\d+)\"")).match(txt);
        if (!mc.hasMatch() || !mp.hasMatch()) {
            if (err)
                *err = QStringLiteral("gem.php Login: нет Code/PersId");
            return false;
        }
        if (code)
            *code = mc.captured(1);
        if (pers)
            *pers = mp.captured(1);
        return true;
    }

    // Импорт без пароля: SSO-cookie GameCenter -> authcode -> refresh + профиль.
    bool mintViaCookies(QString *refresh, qint64 *exp, QString *nick, QString *uid, QString *err)
    {
        QString cerr;
        const auto cookies = VkChromeCookies::decrypt(VkChromeCookies::defaultCefDir(),
                                                      {QStringLiteral("vkplay.ru")}, &cerr);
        if (cookies.isEmpty()) {
            if (err)
                *err = cerr.isEmpty() ? QStringLiteral("не найдена сессия VK Play в GameCenter") : cerr;
            return false;
        }
        QStringList pairs;
        for (const auto &c : cookies)
            pairs << (c.name + QStringLiteral("=") + c.value);
        const QByteArray cookieHeader = pairs.join(QStringLiteral("; ")).toUtf8();

        // authcode
        QNetworkRequest ar{QUrl(API_URL + QStringLiteral("/account/get_gc_auth/?gc_id=0.0"))};
        ar.setRawHeader("Cookie", cookieHeader);
        ar.setRawHeader("X-Requested-With", "XMLHttpRequest");
        ar.setRawHeader("Referer", (QStringLiteral("https://account.vkplay.ru/")).toUtf8());
        int st = 0;
        bool ok = false;
        const QJsonObject aj = QJsonDocument::fromJson(http(ar, nullptr, &st, &ok)).object();
        const QString code = aj.value(QStringLiteral("code")).toString();
        if (code.isEmpty()) {
            if (err)
                *err = QStringLiteral("сессия в GameCenter истекла (HTTP %1)").arg(st);
            return false;
        }

        // обмен кода на токены
        QNetworkRequest tr{QUrl(TOKEN_EXCHANGE_URL)};
        tr.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        tr.setRawHeader("Origin", "https://vkplay.ru");
        tr.setRawHeader("Referer", "https://vkplay.ru/");
        QJsonObject payload;
        payload[QStringLiteral("client_id")] = CLIENT_ID;
        payload[QStringLiteral("grant_type")] = QStringLiteral("authorization_code");
        payload[QStringLiteral("code")] = code;
        const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        const QJsonObject tj = QJsonDocument::fromJson(http(tr, &body, &st, &ok)).object();
        const QString newRefresh = tj.value(QStringLiteral("refresh_token")).toString();
        const QString at = tj.value(QStringLiteral("access_token")).toString();
        if (newRefresh.isEmpty() || at.isEmpty()) {
            if (err)
                *err = QStringLiteral("обмен кода не дал токены (HTTP %1)").arg(st);
            return false;
        }
        if (refresh)
            *refresh = newRefresh;
        if (exp && tj.contains(QStringLiteral("refresh_token_expires_in")))
            *exp = QDateTime::currentSecsSinceEpoch()
                   + static_cast<qint64>(tj.value(QStringLiteral("refresh_token_expires_in")).toDouble());
        gemProfile(at, nick, uid);
        return true;
    }

    void drop(const QString &refresh)
    {
        QNetworkRequest r{QUrl(DROP_URL)};
        r.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        r.setRawHeader("Origin", "https://vkplay.ru");
        r.setRawHeader("Referer", "https://vkplay.ru/");
        QJsonObject payload;
        payload[QStringLiteral("refresh_token")] = refresh;
        payload[QStringLiteral("client_id")] = CLIENT_ID;
        const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        int st = 0;
        bool ok = false;
        http(r, &body, &st, &ok);
    }

    QNetworkAccessManager *m_nam = nullptr;
    VkStore m_store;
};

// ───────────────────────────── фасад-бэкенд ─────────────────────────────────
VkPlayAuthBackend::VkPlayAuthBackend(QObject *parent) : IAuthBackend(parent)
{
    qRegisterMetaType<QVector<Account>>("QVector<Account>");
    m_accounts = VkStore().list(); // синхронно для accounts()

    m_worker = new VkWorker();
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    // Проброс сигналов воркера -> наши (очередь, т.к. воркер в другом потоке).
    connect(m_worker, &VkWorker::progress, this, &IAuthBackend::progress);
    connect(m_worker, &VkWorker::finished, this, &IAuthBackend::finished);
    connect(m_worker, &VkWorker::busyChanged, this, &IAuthBackend::busyChanged);
    connect(m_worker, &VkWorker::error, this, &IAuthBackend::error);
    connect(m_worker, &VkWorker::readyToLaunch, this, &IAuthBackend::readyToLaunch);
    connect(m_worker, &VkWorker::accountsChanged, this, [this](const QVector<Account> &items) {
        m_accounts = items; // держим зеркало для accounts()
        emit accountsChanged(items);
    });

    m_thread.start();
}

VkPlayAuthBackend::~VkPlayAuthBackend()
{
    m_thread.quit();
    m_thread.wait();
}

void VkPlayAuthBackend::setGameDir(const QString &) {} // запуск делает контроллер

void VkPlayAuthBackend::importSession()
{
    QMetaObject::invokeMethod(m_worker, "doImport", Qt::QueuedConnection);
}

void VkPlayAuthBackend::launch(const QString &accountId)
{
    QMetaObject::invokeMethod(m_worker, "doLaunch", Qt::QueuedConnection, Q_ARG(QString, accountId));
}

void VkPlayAuthBackend::refreshSession(const QString &accountId)
{
    QMetaObject::invokeMethod(m_worker, "doRefresh", Qt::QueuedConnection, Q_ARG(QString, accountId));
}

void VkPlayAuthBackend::deleteAccount(const QString &accountId)
{
    QMetaObject::invokeMethod(m_worker, "doDelete", Qt::QueuedConnection, Q_ARG(QString, accountId));
}

#include "VkPlayAuthBackend.moc"
