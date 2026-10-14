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

void HudInjector::attach(qint64 pid, const QString &nick)
{
    if (pid <= 0)
        return;
    if (!dllAvailable()) // нет библиотеки или инжектора — шаг пропускаем молча
        return;
    emit log(QStringLiteral("HUD: жду окно клиента · ") + nick, 0);
    waitForWindow(pid, nick, 0);
}

void HudInjector::waitForWindow(qint64 pid, const QString &nick, int elapsedMs)
{
#ifdef Q_OS_WIN
    if (!processAlive(pid)) {
        emit log(QStringLiteral("HUD: клиент закрылся до подключения · ") + nick, 2);
        return;
    }
    if (hasVisibleWindow(pid)) {
        inject(pid, nick);
        return;
    }
    if (elapsedMs >= kWindowTimeoutMs) {
        emit log(QStringLiteral("HUD: окно клиента не появилось за минуту · ") + nick, 2);
        return;
    }
    QTimer::singleShot(kPollMs, this, [this, pid, nick, elapsedMs] {
        waitForWindow(pid, nick, elapsedMs + kPollMs);
    });
#else
    Q_UNUSED(pid) Q_UNUSED(nick) Q_UNUSED(elapsedMs)
#endif
}

void HudInjector::inject(qint64 pid, const QString &nick)
{
    auto *proc = new QProcess(this);
    proc->setProgram(injectorPath());
    proc->setArguments({QString::number(pid), dllPath()});
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, &QProcess::finished, this,
            [this, proc, nick](int code, QProcess::ExitStatus) {
                const QString out = QString::fromUtf8(proc->readAll()).trimmed();
                const QString line = out.isEmpty() ? QStringLiteral("rfvk_inject завершился с кодом %1").arg(code) : out;
                emit log(QStringLiteral("HUD · ") + nick + QStringLiteral(": ") + line,
                         code == 0 ? 1 : 2);
                proc->deleteLater();
            });
    // Только «не запустился»: при падении/таймауте следом придёт finished,
    // и он сам напишет итог — иначе в журнале была бы двойная строка.
    connect(proc, &QProcess::errorOccurred, this, [this, proc, nick](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        emit log(QStringLiteral("HUD: не удалось запустить rfvk_inject · ") + nick, 2);
        proc->deleteLater();
    });
    proc->start();
}
