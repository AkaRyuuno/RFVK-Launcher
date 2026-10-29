#include "HudInjector.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>

#include "RemoteLoadLibrary.h"
#endif

namespace {
constexpr int kPollMs = 400;       // как часто проверять появление окна
constexpr int kWindowTimeoutMs = 60000; // ждём окно не дольше минуты
constexpr int kLoadPollMs = 50;         // как часто проверять поток LoadLibraryW
constexpr unsigned long kLoadTimeoutMs = 15000; // LoadLibraryW в цели — не дольше 15 с

#ifdef Q_OS_WIN
struct FindData {
    DWORD pid = 0;
    bool found = false;
};

BOOL CALLBACK enumProc(HWND hwnd, LPARAM lparam)
{
    auto *d = reinterpret_cast<FindData *>(lparam);
    DWORD wpid = 0;
    GetWindowThreadProcessId(hwnd, &wpid);
    // Видимое верхнеуровневое окно без владельца — главное окно процесса.
    if (wpid == d->pid && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
        d->found = true;
        return FALSE; // достаточно одного — прекращаем перебор
    }
    return TRUE;
}

bool hasVisibleWindow(qint64 pid)
{
    FindData d;
    d.pid = static_cast<DWORD>(pid);
    EnumWindows(enumProc, reinterpret_cast<LPARAM>(&d));
    return d.found;
}

bool processAlive(qint64 pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h)
        // Отказ в доступе — процесс есть, но защищён (например, античитом).
        // Считаем живым: окно найдём через EnumWindows, а причину неудачи
        // инжекта, если она будет, сообщит загрузка (RemoteLoadLibrary).
        return GetLastError() == ERROR_ACCESS_DENIED;
    DWORD code = 0;
    const bool ok = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return ok;
}
#endif
} // namespace

HudInjector::HudInjector(QObject *parent) : QObject(parent) {}

// Пути — в нативном виде (обратные слэши): LoadLibraryW в цели по документации
// требует именно их.
QString HudInjector::dllPath()
{
    return QDir::toNativeSeparators(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("seirin.dll")));
}

bool HudInjector::dllAvailable()
{
    return QFileInfo::exists(dllPath());
}

QString HudInjector::autoLoginDllPath()
{
    return QDir::toNativeSeparators(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("rfvk_autologin.dll")));
}

bool HudInjector::autoLoginAvailable()
{
    return QFileInfo::exists(autoLoginDllPath());
}

void HudInjector::attach(qint64 pid, const QString &nick)
{
    if (!dllAvailable()) // нет библиотеки — шаг пропускаем молча
        return;
    attachLibrary(pid, dllPath(), QStringLiteral("HUD"), nick);
}

void HudInjector::attachLibrary(qint64 pid, const QString &dll, const QString &tag, const QString &nick)
{
    if (pid <= 0 || !QFileInfo::exists(dll))
        return;
    emit log(tag + QStringLiteral(": жду окно клиента · ") + nick, 0);
    waitForWindow(Job{pid, dll, tag, nick}, 0);
}

void HudInjector::waitForWindow(const Job &job, int elapsedMs)
{
#ifdef Q_OS_WIN
    if (!processAlive(job.pid)) {
        emit log(job.tag + QStringLiteral(": клиент закрылся до подключения · ") + job.nick, 2);
        emit failed(job.pid, job.dll);
        return;
    }
    if (hasVisibleWindow(job.pid)) {
        inject(job);
        return;
    }
    if (elapsedMs >= kWindowTimeoutMs) {
        emit log(job.tag + QStringLiteral(": окно клиента не появилось за минуту · ") + job.nick, 2);
        emit failed(job.pid, job.dll);
        return;
    }
    QTimer::singleShot(kPollMs, this, [this, job, elapsedMs] { waitForWindow(job, elapsedMs + kPollMs); });
#else
    Q_UNUSED(elapsedMs)
    emit failed(job.pid, job.dll);
#endif
}

void HudInjector::inject(const Job &job)
{
#ifdef Q_OS_WIN
    auto load = std::make_shared<remote_load::Job>();
    report(job, remote_load::start(static_cast<unsigned long>(job.pid), job.dll.toStdWString(), *load), load);
#else
    emit failed(job.pid, job.dll);
#endif
}

#ifdef Q_OS_WIN
void HudInjector::report(const Job &job, const remote_load::Result &r, const std::shared_ptr<remote_load::Job> &load)
{
    if (r.step == remote_load::Step::Pending) {
        // Поток LoadLibraryW ещё идёт в клиенте — проверим позже, не блокируя интерфейс.
        QTimer::singleShot(kLoadPollMs, this, [this, job, load] {
            report(job, remote_load::poll(*load, kLoadTimeoutMs), load);
        });
        return;
    }
    const bool ok = r.step == remote_load::Step::Loaded;
    emit log(job.tag + QStringLiteral(" · ") + job.nick + QStringLiteral(": ") + QString::fromStdWString(r.message),
             ok ? 1 : 2);
    if (!ok)
        emit failed(job.pid, job.dll);
}
#endif
