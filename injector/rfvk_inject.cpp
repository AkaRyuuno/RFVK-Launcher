// rfvk_inject — минимальный 32-битный загрузчик DLL в 32-битный процесс клиента.
//
// Зачем отдельный exe: лаунчер 64-битный, а RF_Online.exe 32-битный.
// Классический LoadLibrary-инжект (CreateRemoteThread на LoadLibraryW)
// надёжно работает только из процесса той же разрядности, что и цель —
// адрес LoadLibraryW берётся из своего kernel32 и обязан совпасть с адресом
// в цели (системные DLL делят базу между процессами одной разрядности).
// Поэтому инжект вынесен в этот Win32-процесс; лаунчер зовёт его через
// QProcess, передавая PID цели и путь к DLL. Тот же подход, что у
// tools/inject.ps1 в проекте seirin, только без PowerShell.
//
// Использование:  rfvk_inject.exe <pid> <путь к .dll>
// Коды возврата:  0 — загружена (или уже была);  иначе — не загружена
//                 (конкретная причина выведена в stdout).

#ifndef UNICODE
#define UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

namespace {

// Короткая диагностика в stdout — лаунчер подхватит её в журнал. Пишем в
// UTF-8 напрямую (а не fputws): кодировка консоли иначе искажает кириллицу,
// и лаунчер декодирует вывод как UTF-8.
void say(const wchar_t *fmt, ...)
{
    wchar_t buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    wcscat_s(buf, _countof(buf), L"\n");

    char utf8[2048];
    const int n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (n > 0)
        fwrite(utf8, 1, static_cast<size_t>(n - 1), stdout); // без завершающего \0
    fflush(stdout);
}

// Уже ли загружен модуль с таким именем файла в процессе pid.
// Повторный LoadLibrary лишь увеличил бы счётчик ссылок (DllMain не вызовется),
// поэтому при повторном запуске просто сообщаем и выходим успешно.
bool alreadyLoaded(DWORD pid, const wchar_t *dllName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    bool found = false;
    for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me)) {
        if (_wcsicmp(me.szModule, dllName) == 0) {
            found = true;
            break;
        }
    }
    CloseHandle(snap);
    return found;
}

} // namespace

int wmain(int argc, wchar_t **argv)
{
    if (argc != 3) {
        say(L"использование: rfvk_inject.exe <pid> <путь к dll>");
        return 1;
    }

    const DWORD pid = static_cast<DWORD>(_wtoi(argv[1]));
    const wchar_t *dllPath = argv[2];
    if (pid == 0) {
        say(L"некорректный PID: %ls", argv[1]);
        return 1;
    }

    // Путь к DLL должен существовать и быть абсолютным (LoadLibraryW в чужом
    // процессе резолвит относительные пути от каталога цели, не нашего).
    const DWORD attrs = GetFileAttributesW(dllPath);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        say(L"DLL не найдена: %ls", dllPath);
        return 6;
    }

    // Имя файла без каталога — для проверки «уже загружена».
    const wchar_t *dllName = dllPath;
    for (const wchar_t *p = dllPath; *p; ++p)
        if (*p == L'\\' || *p == L'/')
            dllName = p + 1;

    if (alreadyLoaded(pid, dllName)) {
        say(L"%ls уже загружена в процесс %lu — пропускаю", dllName, pid);
        return 0;
    }

    const DWORD access = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION
                       | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;
    HANDLE proc = OpenProcess(access, FALSE, pid);
    if (!proc) {
        say(L"OpenProcess(%lu) не удался (код %lu) — возможно, нужны права администратора",
            pid, GetLastError());
        return 2;
    }

    int rc = 0;
    void *remote = nullptr;
    HANDLE thread = nullptr;
    // Если поток LoadLibraryW не завершился вовремя, он ещё может читать путь
    // из выделенной памяти — освобождать её нельзя (иначе цель упадёт на
    // обращении к уже освобождённой странице). Оставляем: это десятки байт.
    bool keepRemote = false;
    do {
        // Байты пути (с завершающим нулём) пишем в память цели.
        const SIZE_T bytes = (wcslen(dllPath) + 1) * sizeof(wchar_t);
        remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remote) {
            say(L"VirtualAllocEx не удался (код %lu)", GetLastError());
            rc = 3;
            break;
        }
        if (!WriteProcessMemory(proc, remote, dllPath, bytes, nullptr)) {
            say(L"WriteProcessMemory не удался (код %lu)", GetLastError());
            rc = 3;
            break;
        }

        // Адрес LoadLibraryW в НАШЕМ kernel32 совпадает с адресом в цели:
        // обе — 32-битные, системная DLL делит базу между процессами.
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(k32, "LoadLibraryW"));
        if (!loadLibrary) {
            say(L"не удалось получить адрес LoadLibraryW");
            rc = 4;
            break;
        }

        thread = CreateRemoteThread(proc, nullptr, 0, loadLibrary, remote, 0, nullptr);
        if (!thread) {
            say(L"CreateRemoteThread не удался (код %lu)", GetLastError());
            rc = 4;
            break;
        }

        if (WaitForSingleObject(thread, 15000) != WAIT_OBJECT_0) {
            say(L"LoadLibraryW в цели не завершился за 15 с");
            keepRemote = true;
            rc = 4;
            break;
        }

        // Код выхода потока = HMODULE загруженной DLL (0 — загрузка не удалась,
        // обычно из-за несовпадения разрядности или отсутствия зависимостей).
        DWORD exitCode = 0;
        GetExitCodeThread(thread, &exitCode);
        if (exitCode == 0) {
            say(L"LoadLibraryW вернул NULL — DLL не загрузилась (разрядность/зависимости)");
            rc = 5;
            break;
        }

        say(L"%ls загружена в процесс %lu (база 0x%08lX)", dllName, pid, exitCode);
    } while (false);

    if (thread)
        CloseHandle(thread);
    if (remote && !keepRemote)
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    return rc;
}
