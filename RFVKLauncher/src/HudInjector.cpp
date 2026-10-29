#include "HudInjector.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr int kPollMs = 400;       // как часто проверять появление окна
constexpr int kWindowTimeoutMs = 60000; // ждём окно не дольше минуты

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
        // инжекта, если она будет, честно сообщит сам инжектор.
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

QString HudInjector::injectorPath()
{
    return QDir::toNativeSeparators(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("rfvk_inject.exe")));
}

bool HudInjector::dllFound()
{
    return QFileInfo::exists(dllPath());
}

bool HudInjector::injectorFound()
{
    return QFileInfo::exists(injectorPath());
}

bool HudInjector::dllAvailable()
{
    return dllFound() && injectorFound();
}

QString HudInjector::autoLoginDllPath()
{
    return QDir::toNativeSeparators(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("rfvk_autologin.dll")));
}

bool HudInjector::autoLoginAvailable()
{
    return injectorFound() && QFileInfo::exists(autoLoginDllPath());
}

void HudInjector::attach(qint64 pid, const QString &nick)
{
    if (!dllAvailable()) // нет библиотеки или инжектора — шаг пропускаем молча
        return;
    attachLibrary(pid, dllPath(), QStringLiteral("HUD"), nick);
}

void HudInjector::attachLibrary(qint64 pid, const QString &dll, const QString &tag, const QString &nick)
{
    if (pid <= 0 || !injectorFound() || !QFileInfo::exists(dll))
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
    auto *proc = new QProcess(this);
    proc->setProgram(injectorPath());
    proc->setArguments({QString::number(job.pid), job.dll});
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, &QProcess::finished, this, [this, proc, job](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAll()).trimmed();
        const QString line = out.isEmpty() ? QStringLiteral("rfvk_inject завершился с кодом %1").arg(code) : out;
        emit log(job.tag + QStringLiteral(" · ") + job.nick + QStringLiteral(": ") + line, code == 0 ? 1 : 2);
        if (code != 0)
            emit failed(job.pid, job.dll);
        proc->deleteLater();
    });
    // Только «не запустился»: при падении/таймауте следом придёт finished,
    // и он сам напишет итог — иначе в журнале была бы двойная строка.
    connect(proc, &QProcess::errorOccurred, this, [this, proc, job](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        emit log(job.tag + QStringLiteral(": не удалось запустить rfvk_inject · ") + job.nick, 2);
        emit failed(job.pid, job.dll);
        proc->deleteLater();
    });
    proc->start();
}
