#include "Controller.h"
#include "AutoLoginSession.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTime>
#include <QTimer>

namespace {
// «1 аккаунт», «2 аккаунта», «5 аккаунтов».
QString accountsWord(int n)
{
    const int m10 = n % 10, m100 = n % 100;
    if (m10 == 1 && m100 != 11)
        return QStringLiteral("%1 аккаунт").arg(n);
    if (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14))
        return QStringLiteral("%1 аккаунта").arg(n);
    return QStringLiteral("%1 аккаунтов").arg(n);
}

// Раса × 2 + пол (как у клиента) → название расы.
QString raceName(int race)
{
    switch (race / 2) {
    case 0: return QStringLiteral("Беллато");
    case 1: return QStringLiteral("Кора");
    case 2: return QStringLiteral("Акретия");
    default: return QString();
    }
}

// FG-пароль клиента: печатные ASCII-символы, не длиннее 12 (буфер запроса — 13 байт).
constexpr int kFgMaxLen = 12;
bool validFg(const QString &fg)
{
    if (fg.size() > kFgMaxLen)
        return false;
    for (const QChar ch : fg)
        if (ch.unicode() < 0x21 || ch.unicode() > 0x7E)
            return false;
    return true;
}
} // namespace

Controller::Controller(AccountModel *model, IAuthBackend *backend, QObject *parent)
    : QObject(parent), m_model(model), m_backend(backend),
      m_settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("RFVK"),
                 QStringLiteral("RFVKLauncher"))
{
    // Настройки игрока: каталог игры, тема, выбранные аккаунты, HUD.
    m_gameDir = m_settings.value(QStringLiteral("gameDir")).toString();
    m_theme = m_settings.value(QStringLiteral("theme"), QStringLiteral("dark")).toString();
    m_hudEnabled = m_settings.value(QStringLiteral("hudEnabled"), false).toBool();
    m_autoLoginEnabled = m_settings.value(QStringLiteral("autoLogin"), true).toBool();

    // Подключение HUD: свои строки пишет в общий журнал.
    m_hud = new HudInjector(this);
    connect(m_hud, &HudInjector::log, this, &Controller::appendLog);
    // Библиотека автовхода не подключилась — его сессии ждать нечего.
    connect(m_hud, &HudInjector::failed, this, [this](qint64 pid, const QString &dll) {
        if (dll != HudInjector::autoLoginDllPath())
            return;
        if (AutoLoginSession *s = m_autoSessions.value(pid))
            s->abandon();
    });
    QStringList saved = m_settings.value(QStringLiteral("selection")).toStringList();
    if (saved.isEmpty()) // совместимость с прежним одиночным выбором
        saved = QStringList{m_settings.value(QStringLiteral("lastLoginId")).toString()};

    m_model->reset(m_backend->accounts());
    connectBackend();
    recomputeGameStatus();

    // Восстанавливаем выбор; если ничего не сохранилось — первый аккаунт.
    m_selected = inModelOrder(saved);
    if (m_selected.isEmpty() && m_model->rowCount() > 0)
        m_selected = {m_model->at(0).id};
    m_anchor = m_selected.isEmpty() ? QString() : m_selected.first();
}

void Controller::connectBackend()
{
    connect(m_backend, &IAuthBackend::progress, this, [this](const QString &step, int level) {
        appendLog(step, level);
        if (busy()) {
            m_progressStep = step;
            emit progressChanged();
        }
    });

    // Конец операции бэкенда (busy -> false) — подаём следующую из очереди.
    connect(m_backend, &IAuthBackend::busyChanged, this, [this](bool b) {
        m_backendBusy = b;
        if (!b)
            opDone();
        updateBusy();
    });

    connect(m_backend, &IAuthBackend::stepProgress, this, [this](int index, int total) {
        m_progressIndex = index;
        m_progressTotal = total;
        emit progressChanged();
    });

    connect(m_backend, &IAuthBackend::accountsChanged, this, [this](const QVector<Account> &items) {
        m_model->reset(items);
        // Сохранить выбор по id (исчезнувшие — убрать); пустой — первый аккаунт.
        QStringList keep = inModelOrder(m_selected);
        if (keep.isEmpty() && m_model->rowCount() > 0)
            keep = {m_model->at(0).id};
        m_selected = keep;
        // Ошибки удалённых аккаунтов больше не нужны.
        for (auto it = m_errors.begin(); it != m_errors.end();)
            it = m_model->indexOfId(it.key()) < 0 ? m_errors.erase(it) : std::next(it);
        emit selectionChanged();
        emit errorChanged();
        emit accountsCountChanged();
    });

    connect(m_backend, &IAuthBackend::readyToLaunch, this,
            [this](const QString &accountId, const QString &persId, const QString &code) {
                startGame(accountId, persId, code);
            });

    connect(m_backend, &IAuthBackend::error, this, [this](const QString &msg) {
        emit toast(msg, 2);
        // Ошибка относится к аккаунту текущей операции — панель покажет баннер.
        if (m_current.kind != OpKind::Import)
            setError(m_current.id, msg);
    });

    connect(m_backend, &IAuthBackend::finished, this, [this](bool ok, const QString &msg) {
        if (ok) {
            ++m_batchOk;
            clearError(m_current.id);
            // Удалённому аккаунту настройки автовхода больше не нужны.
            if (m_current.kind == OpKind::Delete)
                m_autoStore.remove(m_current.id);
            // В пакете успешные тосты заменяются одной сводкой в конце.
            if (m_batchTotal <= 1)
                emit toast(msg, 1);
        }
        // Если бэкенд завершил операцию, не переходя в busy (мгновенный отказ),
        // очередь продолжаем отсюда.
        if (!m_backendBusy)
            opDone();
    });
}

// ───────────────────────────── выбор аккаунтов ──────────────────────────────

QStringList Controller::inModelOrder(const QStringList &ids) const
{
    QStringList out;
    for (const Account &a : m_model->items())
        if (ids.contains(a.id))
            out << a.id;
    return out;
}

void Controller::setSelection(const QStringList &ids)
{
    const QStringList next = inModelOrder(ids);
    if (next == m_selected)
        return;
    m_selected = next;
    m_settings.setValue(QStringLiteral("selection"), m_selected);
    emit selectionChanged();
    emit errorChanged(); // errorText зависит от выбора
}

void Controller::selectById(const QString &id)
{
    if (m_model->indexOfId(id) < 0)
        return;
    m_anchor = id;
    setSelection({id});
}

void Controller::toggleSelect(const QString &id)
{
    if (m_model->indexOfId(id) < 0)
        return;
    QStringList next = m_selected;
    if (next.contains(id)) {
        if (next.size() == 1) // хотя бы один аккаунт остаётся выбранным
            return;
        next.removeAll(id);
    } else {
        next << id;
    }
    m_anchor = id;
    setSelection(next);
}

void Controller::selectRange(const QString &id)
{
    const int to = m_model->indexOfId(id);
    if (to < 0)
        return;
    int from = m_model->indexOfId(m_anchor);
    if (from < 0)
        from = to;
    QStringList next;
    for (int i = qMin(from, to); i <= qMax(from, to); ++i)
        next << m_model->at(i).id;
    setSelection(next); // якорь не меняем — как в проводнике
}

void Controller::selectAll()
{
    QStringList all;
    for (const Account &a : m_model->items())
        all << a.id;
    setSelection(all);
}

QVariantMap Controller::accountMap(const Account &a) const
{
    QVariantMap m;
    m[QStringLiteral("valid")] = true;
    m[QStringLiteral("id")] = a.id;
    m[QStringLiteral("login")] = a.login;
    m[QStringLiteral("loginKind")] = a.loginKind;
    m[QStringLiteral("nick")] = a.nick;
    m[QStringLiteral("uid")] = a.uid;
    m[QStringLiteral("persId")] = a.persId;
    m[QStringLiteral("sessionState")] = a.sessionState;
    m[QStringLiteral("sessionExpiry")] = a.sessionExpiry;
    m[QStringLiteral("sessionRefreshed")] = a.sessionRefreshed;
    m[QStringLiteral("initials")] = AccountModel::initialsOf(a.nick.isEmpty() ? a.login : a.nick);
    m[QStringLiteral("error")] = m_errors.value(a.id);
    // Автовход: задан ли FG (сам пароль в QML не попадает), сохранённые
    // персонажи и выбранный.
    m[QStringLiteral("fgSet")] = m_autoStore.hasFg(a.id);
    QVariantList chars;
    for (const SavedCharacter &c : m_autoStore.characters(a.id)) {
        QVariantMap cm;
        cm[QStringLiteral("slot")] = c.slot;
        cm[QStringLiteral("name")] = c.name;
        cm[QStringLiteral("level")] = c.level;
        cm[QStringLiteral("race")] = raceName(c.race);
        chars << cm;
    }
    m[QStringLiteral("characters")] = chars;
    m[QStringLiteral("autoChar")] = m_autoStore.chosenName(a.id);
    return m;
}

QVariantMap Controller::selectedAccount() const
{
    const int idx = m_model->indexOfId(selectedId());
    if (idx < 0)
        return {{QStringLiteral("valid"), false}};
    return accountMap(m_model->at(idx));
}

QVariantList Controller::selectedAccounts() const
{
    QVariantList out;
    for (const QString &id : m_selected) {
        const int idx = m_model->indexOfId(id);
        if (idx >= 0)
            out << accountMap(m_model->at(idx));
    }
    return out;
}

// ───────────────────────────── ошибки ───────────────────────────────────────

void Controller::setError(const QString &id, const QString &msg)
{
    if (id.isEmpty())
        return;
    m_errors[id] = msg;
    emit errorChanged();
    emit selectionChanged(); // selectedAccounts несут текст ошибки
}

void Controller::clearError(const QString &id)
{
    if (m_errors.remove(id)) {
        emit errorChanged();
        emit selectionChanged();
    }
}

void Controller::dismissError()
{
    for (const QString &id : m_selected)
        m_errors.remove(id);
    emit errorChanged();
    emit selectionChanged();
}

// ───────────────────────────── очередь операций ─────────────────────────────

void Controller::enqueue(OpKind kind, const QStringList &ids)
{
    if (ids.isEmpty())
        return;
    // Новый пакет, если очередь пуста и ничего не выполняется.
    if (!m_inFlight && m_queue.isEmpty()) {
        m_batchIndex = 0;
        m_batchTotal = 0;
        m_batchOk = 0;
    }
    for (const QString &id : ids)
        m_queue.append({kind, id});
    m_batchTotal += ids.size();
    if (m_batchTotal > 1) {
        static const char *verbs[] = {"Импорт", "Запуск", "Обновление сессий", "Удаление"};
        appendLog(QString::fromUtf8(verbs[int(kind)]) + QStringLiteral(": ") + accountsWord(ids.size()), 0);
    }
    updateBusy();
    emit progressChanged();
    schedulePump();
}

void Controller::schedulePump()
{
    if (m_pumpScheduled)
        return;
    m_pumpScheduled = true;
    // Через очередь событий: даём бэкенду дослать сигналы предыдущей операции.
    QTimer::singleShot(0, this, &Controller::pump);
}

void Controller::pump()
{
    m_pumpScheduled = false;
    if (m_inFlight || m_backendBusy)
        return;

    while (!m_queue.isEmpty()) {
        const Op op = m_queue.takeFirst();
        ++m_batchIndex;
        // Аккаунт мог исчезнуть (удалён предыдущей операцией) — пропускаем.
        if (op.kind != OpKind::Import && m_model->indexOfId(op.id) < 0)
            continue;

        m_current = op;
        m_inFlight = true;
        m_progressStep.clear();
        m_progressIndex = 0;
        m_progressTotal = 0;
        emit progressChanged();
        updateBusy();

        switch (op.kind) {
        case OpKind::Import: m_backend->importSession(); break;
        case OpKind::Launch: m_backend->launch(op.id); break;
        case OpKind::Refresh: m_backend->refreshSession(op.id); break;
        case OpKind::Delete: m_backend->deleteAccount(op.id); break;
        }
        return;
    }

    // Очередь пуста — пакет завершён.
    if (m_batchTotal > 1) {
        const QString msg = QStringLiteral("Готово: %1 из %2").arg(m_batchOk).arg(m_batchTotal);
        const int level = m_batchOk == m_batchTotal ? 1 : 2;
        appendLog(msg, level);
        emit toast(msg, level == 1 ? 1 : 2);
    }
    m_batchIndex = 0;
    m_batchTotal = 0;
    m_batchOk = 0;
    emit progressChanged();
    updateBusy();
}

void Controller::opDone()
{
    if (!m_inFlight)
        return;
    m_inFlight = false;
    schedulePump();
    updateBusy();
}

void Controller::updateBusy()
{
    // Между операциями пакета UI остаётся заблокированным (очередь не пуста).
    const bool b = busy() || m_pumpScheduled;
    if (b == m_busyCache)
        return;
    m_busyCache = b;
    emit busyChanged();
}

// ───────────────────────────── действия ─────────────────────────────────────

void Controller::importSession()
{
    enqueue(OpKind::Import, {QString()});
}

bool Controller::checkGameDir()
{
    if (m_gameDir.trimmed().isEmpty()) {
        appendLog(QStringLiteral("Не задан каталог игры"), 2);
        emit toast(QStringLiteral("Не задан каталог игры"), 2);
        return false;
    }
    if (!m_gameDirValid) {
        const QString msg = QStringLiteral("RF_Online.exe не найден: ") + QDir::toNativeSeparators(m_gameDir);
        appendLog(msg, 2);
        emit toast(msg, 2);
        return false;
    }
    return true;
}

// Запуск с локальной проверкой окружения ДО обращения к бэкенду.
void Controller::launch(const QString &id)
{
    if (id.isEmpty()) {
        emit toast(QStringLiteral("Сначала выберите аккаунт"), 2);
        return;
    }
    if (checkGameDir())
        enqueue(OpKind::Launch, {id});
}

void Controller::launchSelected()
{
    if (m_selected.isEmpty()) {
        emit toast(QStringLiteral("Сначала выберите аккаунт"), 2);
        return;
    }
    if (checkGameDir())
        enqueue(OpKind::Launch, m_selected);
}

void Controller::refresh(const QString &id)
{
    if (!id.isEmpty())
        enqueue(OpKind::Refresh, {id});
}

void Controller::refreshSelected()
{
    enqueue(OpKind::Refresh, m_selected);
}

void Controller::deleteAccount(const QString &id)
{
    if (!id.isEmpty())
        enqueue(OpKind::Delete, {id});
}

void Controller::deleteSelected()
{
    enqueue(OpKind::Delete, m_selected);
}

// ───────────────────────────── настройки ────────────────────────────────────

void Controller::setGameDir(const QString &dir)
{
    const QString clean = QDir::fromNativeSeparators(dir).trimmed();
    if (m_gameDir == clean)
        return;
    m_gameDir = clean;
    m_settings.setValue(QStringLiteral("gameDir"), m_gameDir);
    recomputeGameStatus();
    if (m_gameDirValid)
        appendLog(QStringLiteral("Каталог игры проверен: RF_Online.exe найден"), 1);
    else if (!m_gameDir.isEmpty())
        appendLog(QStringLiteral("RF_Online.exe не найден в каталоге игры"), 2);
    emit gameStatusChanged();
}

void Controller::setTheme(const QString &t)
{
    if (m_theme == t)
        return;
    m_theme = t;
    m_settings.setValue(QStringLiteral("theme"), t);
    emit themeChanged();
}

void Controller::setHudEnabled(bool on)
{
    if (m_hudEnabled == on)
        return;
    m_hudEnabled = on;
    m_settings.setValue(QStringLiteral("hudEnabled"), on);
    emit hudChanged();
}

void Controller::setAutoLoginEnabled(bool on)
{
    if (m_autoLoginEnabled == on)
        return;
    m_autoLoginEnabled = on;
    m_settings.setValue(QStringLiteral("autoLogin"), on);
    emit autoLoginChanged();
}

void Controller::setAccountFg(const QString &id, const QString &fg)
{
    if (m_model->indexOfId(id) < 0)
        return;
    const QString clean = fg.trimmed();
    if (!validFg(clean)) {
        emit toast(QStringLiteral("FG-пароль: латиница, цифры и знаки, до %1 символов").arg(kFgMaxLen), 2);
        return;
    }
    m_autoStore.setFg(id, clean);
    emit toast(clean.isEmpty() ? QStringLiteral("FG-пароль удалён — вводите вручную")
                               : QStringLiteral("FG-пароль сохранён"),
               1);
    emit selectionChanged(); // selectedAccount несёт fgSet
}

void Controller::setAccountCharacter(const QString &id, const QString &name)
{
    if (m_model->indexOfId(id) < 0)
        return;
    int slot = -1;
    if (!name.isEmpty()) {
        for (const SavedCharacter &c : m_autoStore.characters(id))
            if (c.name == name)
                slot = c.slot;
        if (slot < 0)
            return; // не из сохранённого списка
    }
    m_autoStore.setChosen(id, name, slot);
    emit selectionChanged();
}

void Controller::startAutoLogin(qint64 pid, const QString &accountId, const QString &nick)
{
    if (pid <= 0 || !m_autoLoginEnabled || !HudInjector::autoLoginAvailable())
        return;
    if (AutoLoginSession *old = m_autoSessions.take(pid)) // PID переиспользован
        old->deleteLater();

    AutoLoginSession::Config cfg;
    cfg.accountId = accountId;
    cfg.nick = nick;
    cfg.fg = m_autoStore.fg(accountId);
    cfg.charName = m_autoStore.chosenName(accountId);
    cfg.charSlot = m_autoStore.chosenSlot(accountId);

    auto *session = new AutoLoginSession(pid, cfg, this);
    cfg.fg.fill(char(0));
    connect(session, &AutoLoginSession::log, this, &Controller::appendLog);
    connect(session, &AutoLoginSession::charactersFound, this, &Controller::onCharactersFound);
    connect(session, &AutoLoginSession::ended, this, [this](qint64 endedPid) {
        if (AutoLoginSession *s = m_autoSessions.take(endedPid))
            s->deleteLater();
    });
    if (!session->open()) {
        session->deleteLater();
        return;
    }
    m_autoSessions.insert(pid, session);

    QStringList steps{QStringLiteral("сервер"), QStringLiteral("«Начать игру»")};
    steps << (m_autoStore.hasFg(accountId) ? QStringLiteral("FG") : QStringLiteral("FG — вручную"));
    steps << (cfg.charName.isEmpty() ? QStringLiteral("персонаж — вручную") : QStringLiteral("персонаж ") + cfg.charName);
    appendLog(QStringLiteral("Автовход: ") + steps.join(QStringLiteral(" → ")) + QStringLiteral(" · ") + nick, 0);
    m_hud->attachLibrary(pid, HudInjector::autoLoginDllPath(), QStringLiteral("Автовход"), nick);
}

void Controller::onCharactersFound(const QString &accountId, const QList<SavedCharacter> &list)
{
    if (m_model->indexOfId(accountId) < 0)
        return;
    m_autoStore.setCharacters(accountId, list);
    // Выбранного персонажа больше нет на аккаунте (удалён/переименован) — сбросить выбор.
    const QString chosen = m_autoStore.chosenName(accountId);
    if (!chosen.isEmpty()) {
        bool still = false;
        for (const SavedCharacter &c : list)
            if (c.name == chosen) {
                still = true;
                m_autoStore.setChosen(accountId, c.name, c.slot); // слот мог смениться
            }
        if (!still)
            m_autoStore.setChosen(accountId, QString(), -1);
    }
    QStringList names;
    for (const SavedCharacter &c : list)
        names << c.name;
    appendLog(QStringLiteral("Персонажи аккаунта сохранены: ")
                  + (names.isEmpty() ? QStringLiteral("нет") : names.join(QStringLiteral(", "))),
              1);
    emit selectionChanged();
}

void Controller::recomputeGameStatus()
{
    m_gameDirValid = !m_gameDir.isEmpty()
                     && QFileInfo::exists(QDir(m_gameDir).filePath(QStringLiteral("RF_Online.exe")));
    // GameCenter в стандартном пути %LOCALAPPDATA%\GameCenter.
    const QString local = qEnvironmentVariable("LOCALAPPDATA");
    m_gameCenterFound = !local.isEmpty() && QFileInfo::exists(QDir(local).filePath(QStringLiteral("GameCenter")));
    if (qEnvironmentVariableIsSet("RFVK_NOGC")) // для скриншота состояния «GameCenter не найден»
        m_gameCenterFound = false;
}

void Controller::appendLog(const QString &text, int level)
{
    emit logLine(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), text, level);
}

// Фактический запуск клиента. Аргументы — как у GameCenter для RF Online:
// RF_Online.exe --sz_token=<code> --sz_pers_id=<persId> -my_ui_locale_arg=ru-RU
//               -my_ui_currency_arg=RUB
void Controller::startGame(const QString &accountId, const QString &persId, const QString &code)
{
    const int idx = m_model->indexOfId(accountId);
    const QString nick = idx >= 0 ? m_model->at(idx).nick : accountId;

    const QString exe = QDir(m_gameDir).filePath(QStringLiteral("RF_Online.exe"));
    if (m_gameDir.isEmpty() || !QFileInfo::exists(exe)) {
        const QString msg = QStringLiteral("RF_Online.exe не найден: ") + QDir::toNativeSeparators(exe);
        appendLog(msg, 2);
        emit toast(msg, 2);
        setError(accountId, msg);
        return;
    }

    const QStringList args = {
        QStringLiteral("--sz_token=") + code,
        QStringLiteral("--sz_pers_id=") + persId,
        QStringLiteral("-my_ui_locale_arg=ru-RU"),
        QStringLiteral("-my_ui_currency_arg=RUB"),
    };

    // В лог пишем команду с замаскированным токеном.
    appendLog(QStringLiteral("Запускаю: RF_Online.exe --sz_token=<скрыт> --sz_pers_id=") + persId
                  + QStringLiteral(" -my_ui_locale_arg=ru-RU -my_ui_currency_arg=RUB"),
              0);

    // Каждый клиент — отдельный независимый процесс (можно несколько сразу).
    qint64 pid = 0;
    const bool ok = QProcess::startDetached(exe, args, m_gameDir, &pid);
    if (ok) {
        ++m_batchOk;
        clearError(accountId); // успешный запуск снимает прежнюю ошибку
        appendLog(QStringLiteral("RF Online запущен · ") + nick, 1);
        if (m_batchTotal <= 1)
            emit toast(QStringLiteral("RF Online запущен · ") + nick, 1);
        // По настройке — подключить улучшенный HUD к этому клиенту (не срывает
        // запуск при неудаче; пропускается, если библиотеки нет рядом с лаунчером).
        if (m_hudEnabled && HudInjector::dllAvailable())
            m_hud->attach(pid, nick);
        // Автовход: сервер, «Начать игру», FG и персонаж — за игрока.
        startAutoLogin(pid, accountId, nick);
    } else {
        const QString msg = QStringLiteral("Не удалось запустить RF_Online.exe");
        appendLog(msg, 2);
        emit toast(msg, 2);
        setError(accountId, msg);
    }
}
