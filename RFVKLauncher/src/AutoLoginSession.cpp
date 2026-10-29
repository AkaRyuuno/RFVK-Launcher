#include "AutoLoginSession.h"

#include "rfvk_autologin_shared.h"

#include <cstring>
#include <string>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace al = rfvk_autologin;

namespace {
constexpr int kPollMs = 300;
// Библиотека открывает память сразу после подключения; подключение ждёт окно
// клиента до минуты — с запасом.
constexpr qint64 kAttachTimeoutMs = 3 * 60 * 1000;
// Дольше экран входа не длится (у библиотеки свой предел — 15 минут).
constexpr qint64 kSessionTimeoutMs = 20 * 60 * 1000;

#ifdef Q_OS_WIN
bool processAlive(qint64 pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h)
        return GetLastError() == ERROR_ACCESS_DENIED; // защищён — значит жив
    DWORD code = 0;
    const bool ok = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return ok;
}
#endif

QString fromUtf16(const wchar_t *s, int cap)
{
    int n = 0;
    while (n < cap && s[n])
        ++n;
    return QString::fromWCharArray(s, n);
}
} // namespace

AutoLoginSession::AutoLoginSession(qint64 pid, Config config, QObject *parent)
    : QObject(parent), m_pid(pid), m_cfg(std::move(config))
{
    m_timer.setInterval(kPollMs);
    connect(&m_timer, &QTimer::timeout, this, &AutoLoginSession::poll);
}

AutoLoginSession::~AutoLoginSession()
{
#ifdef Q_OS_WIN
    if (m_view)
        UnmapViewOfFile(m_view);
    if (m_mapping)
        CloseHandle(static_cast<HANDLE>(m_mapping));
#endif
    m_cfg.fg.fill('\0');
}

bool AutoLoginSession::open()
{
#ifdef Q_OS_WIN
    const std::wstring name = std::wstring(al::kMappingPrefix) + std::to_wstring(m_pid);
    // Безопасность по умолчанию: доступ — у текущего пользователя.
    HANDLE h = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                  sizeof(al::Shared), name.c_str());
    if (!h) {
        emit log(QStringLiteral("Автовход: не удалось создать общую память (код %1)").arg(GetLastError()), 2);
        return false;
    }
    auto *view = static_cast<al::Shared *>(MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(al::Shared)));
    if (!view) {
        CloseHandle(h);
        emit log(QStringLiteral("Автовход: не удалось открыть общую память"), 2);
        return false;
    }
    m_mapping = h;
    m_view = view;

    std::memset(view, 0, sizeof(al::Shared));
    view->magic = al::kMagic;
    view->version = al::kVersion;
    view->size = sizeof(al::Shared);
    view->flags = al::SelectServer | al::PressStart;
    if (!m_cfg.fg.isEmpty())
        view->flags |= al::EnterFg;
    if (!m_cfg.charName.isEmpty())
        view->flags |= al::SelectChar;
    view->worldIndex = m_cfg.worldIndex;
    view->charSlot = m_cfg.charSlot;
    const int nameLen = static_cast<int>(qMin<qsizetype>(m_cfg.charName.size(), al::kNameLen - 1));
    m_cfg.charName.left(nameLen).toWCharArray(view->charName);
    view->charName[nameLen] = 0;
    const int fgLen = static_cast<int>(qMin<qsizetype>(m_cfg.fg.size(), al::kFgLen - 1));
    std::memcpy(view->fg, m_cfg.fg.constData(), static_cast<size_t>(fgLen));
    view->fg[fgLen] = 0;
    m_cfg.fg.fill('\0'); // пароль теперь только в общей памяти, до чтения библиотекой

    m_age.start();
    m_timer.start();
    return true;
#else
    return false;
#endif
}

void AutoLoginSession::abandon()
{
    if (m_view && m_view->dllState == al::NotAttached)
        std::memset(m_view->fg, 0, sizeof(m_view->fg));
    end();
}

void AutoLoginSession::drainEvents()
{
    const int total = m_view->eventCount;
    if (total - m_seenEvents > al::kEvents) // отстали больше чем на кольцо — старые потеряны
        m_seenEvents = total - al::kEvents;
    for (; m_seenEvents < total; ++m_seenEvents) {
        const al::Event &e = m_view->events[m_seenEvents % al::kEvents];
        const QString text = fromUtf16(e.text, al::kEventLen);
        if (!text.isEmpty())
            emit log(text + QStringLiteral(" · ") + m_cfg.nick, static_cast<int>(e.level));
    }
}

void AutoLoginSession::poll()
{
    if (!m_view || m_ended)
        return;

    drainEvents();

    if (m_view->charsReady && !m_charsTaken) {
        m_charsTaken = true;
        QList<SavedCharacter> list;
        for (int i = 0; i < al::kSlots; ++i) {
            const al::CharInfo &c = m_view->chars[i];
            if (!c.used)
                continue;
            SavedCharacter s;
            s.slot = i;
            s.name = fromUtf16(c.name, al::kNameLen);
            s.level = static_cast<int>(c.level);
            s.race = static_cast<int>(c.race);
            if (!s.name.isEmpty())
                list << s;
        }
        emit charactersFound(m_cfg.accountId, list);
    }

    const int state = m_view->dllState;
    if (state == al::Finished || state == al::Failed) {
        end();
        return;
    }
#ifdef Q_OS_WIN
    if (!processAlive(m_pid)) {
        end();
        return;
    }
#endif
    if (state == al::NotAttached && m_age.elapsed() > kAttachTimeoutMs) {
        emit log(QStringLiteral("Автовход: библиотека не подключилась к клиенту · ") + m_cfg.nick, 2);
        abandon();
        return;
    }
    if (m_age.elapsed() > kSessionTimeoutMs)
        end();
}

void AutoLoginSession::end()
{
    if (m_ended)
        return;
    m_ended = true;
    m_timer.stop();
    if (m_view)
        drainEvents();
    emit ended(m_pid);
}
