#include "RemoteLoadLibrary.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <cwchar>

namespace remote_load {

namespace {

std::wstring format(const wchar_t *fmt, ...)
{
    wchar_t buf[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    return buf;
}

Result failed(std::wstring message) { return {Step::Failed, std::move(message)}; }

template <class T>
bool readRemote(HANDLE proc, std::uint64_t addr, T &out)
{
    SIZE_T n = 0;
    return ReadProcessMemory(proc, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(addr)), &out,
                             sizeof(T), &n)
           && n == sizeof(T);
}

// Модули цели, нужные для загрузки.
struct Modules {
    bool dllLoaded = false;        // DLL с таким именем уже в процессе
    std::uint64_t kernel32 = 0;    // база 32-битного kernel32.dll (0 — не найден)
};

// Снимок модулей. Сразу после старта процесса снимок может не получиться
// (ERROR_BAD_LENGTH / ERROR_PARTIAL_COPY, пока загрузчик меняет список) —
// несколько повторов.
bool scanModules(DWORD pid, const wchar_t *dllName, Modules &out, DWORD &error)
{
    HANDLE snap = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 5; ++attempt) {
        snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap != INVALID_HANDLE_VALUE)
            break;
        error = GetLastError();
        if (error != ERROR_BAD_LENGTH && error != ERROR_PARTIAL_COPY)
            return false;
        Sleep(50);
    }
    if (snap == INVALID_HANDLE_VALUE)
        return false;

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me)) {
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(me.modBaseAddr));
        if (_wcsicmp(me.szModule, dllName) == 0)
            out.dllLoaded = true;
        // В WOW64-процессе два kernel32.dll: 64-битный (выше 4 ГБ) и нужный
        // нам 32-битный. Разрядность образа проверит findLoadLibraryW.
        if (_wcsicmp(me.szModule, L"kernel32.dll") == 0 && base < 0x100000000ull)
            out.kernel32 = base;
    }
    CloseHandle(snap);
    return true;
}

// Адрес LoadLibraryW в 32-битном kernel32 цели — по его таблице экспорта.
// Имена в таблице отсортированы, поэтому — двоичный поиск.
bool findLoadLibraryW(HANDLE proc, std::uint64_t base, std::uint32_t &address, std::wstring &why)
{
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS32 nt{};
    if (!readRemote(proc, base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE
        || !readRemote(proc, base + static_cast<std::uint32_t>(dos.e_lfanew), nt)
        || nt.Signature != IMAGE_NT_SIGNATURE) {
        why = L"не удалось прочитать заголовок kernel32.dll цели";
        return false;
    }
    if (nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386
        || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        why = L"клиент не 32-битный";
        return false;
    }

    const IMAGE_DATA_DIRECTORY dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    IMAGE_EXPORT_DIRECTORY exp{};
    if (!dir.VirtualAddress || !readRemote(proc, base + dir.VirtualAddress, exp)) {
        why = L"не удалось прочитать экспорт kernel32.dll цели";
        return false;
    }

    static const char kName[] = "LoadLibraryW";
    long lo = 0;
    long hi = static_cast<long>(exp.NumberOfNames) - 1;
    while (lo <= hi) {
        const long mid = (lo + hi) / 2;
        std::uint32_t nameRva = 0;
        char name[sizeof(kName) + 1] = {};
        if (!readRemote(proc, base + exp.AddressOfNames + 4ull * mid, nameRva)) {
            why = L"не удалось прочитать экспорт kernel32.dll цели";
            return false;
        }
        // Имя может оказаться у конца страницы — читаем короткими кусками.
        SIZE_T n = 0;
        ReadProcessMemory(proc, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(base + nameRva)), name,
                          sizeof(name) - 1, &n);
        name[n] = 0;
        const int cmp = std::strcmp(name, kName);
        if (cmp < 0) {
            lo = mid + 1;
        } else if (cmp > 0) {
            hi = mid - 1;
        } else {
            std::uint16_t ordinal = 0;
            std::uint32_t funcRva = 0;
            if (!readRemote(proc, base + exp.AddressOfNameOrdinals + 2ull * mid, ordinal)
                || ordinal >= exp.NumberOfFunctions
                || !readRemote(proc, base + exp.AddressOfFunctions + 4ull * ordinal, funcRva)) {
                why = L"не удалось прочитать экспорт kernel32.dll цели";
                return false;
            }
            // Адрес внутри каталога экспорта — переадресация («DLL.Функция»), а не код.
            if (funcRva >= dir.VirtualAddress && funcRva < dir.VirtualAddress + dir.Size) {
                why = L"LoadLibraryW в kernel32.dll цели переадресован — не поддерживается";
                return false;
            }
            address = static_cast<std::uint32_t>(base + funcRva);
            return true;
        }
    }
    why = L"LoadLibraryW не найден в kernel32.dll цели";
    return false;
}

} // namespace

Job::~Job()
{
    if (m_thread) {
        if (m_remotePath && WaitForSingleObject(m_thread, 0) == WAIT_OBJECT_0)
            VirtualFreeEx(m_process, m_remotePath, 0, MEM_RELEASE);
        CloseHandle(m_thread);
    } else if (m_remotePath) {
        VirtualFreeEx(m_process, m_remotePath, 0, MEM_RELEASE);
    }
    if (m_process)
        CloseHandle(m_process);
}

Result start(unsigned long pid, const std::wstring &dllPath, Job &job)
{
    // LoadLibraryW в чужом процессе резолвит относительный путь от каталога
    // цели — путь обязан быть абсолютным и существовать.
    const DWORD attrs = GetFileAttributesW(dllPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
        return failed(format(L"DLL не найдена: %ls", dllPath.c_str()));

    const size_t slash = dllPath.find_last_of(L"\\/");
    job.m_dllName = slash == std::wstring::npos ? dllPath : dllPath.substr(slash + 1);
    job.m_pid = pid;

    Modules mods;
    DWORD snapError = 0;
    if (!scanModules(pid, job.m_dllName.c_str(), mods, snapError))
        return failed(format(L"не удалось получить список модулей процесса %lu (код %lu)", pid, snapError));
    if (mods.dllLoaded)
        return {Step::Loaded, format(L"%ls уже загружена в процесс %lu — пропускаю", job.m_dllName.c_str(), pid)};
    if (!mods.kernel32)
        return failed(L"в клиенте не найден 32-битный kernel32.dll — клиент не 32-битный?");

    const DWORD access = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION
                         | PROCESS_VM_WRITE | PROCESS_VM_READ;
    job.m_process = OpenProcess(access, FALSE, pid);
    if (!job.m_process)
        return failed(format(L"OpenProcess(%lu) не удался (код %lu) — возможно, нужны права администратора",
                             pid, GetLastError()));

    std::uint32_t loadLibrary = 0;
    std::wstring why;
    if (!findLoadLibraryW(job.m_process, mods.kernel32, loadLibrary, why))
        return failed(why);

    const SIZE_T bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    job.m_remotePath = VirtualAllocEx(job.m_process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!job.m_remotePath)
        return failed(format(L"VirtualAllocEx не удался (код %lu)", GetLastError()));
    if (!WriteProcessMemory(job.m_process, job.m_remotePath, dllPath.c_str(), bytes, nullptr))
        return failed(format(L"WriteProcessMemory не удался (код %lu)", GetLastError()));

    job.m_thread = CreateRemoteThread(job.m_process, nullptr, 0,
                                      reinterpret_cast<LPTHREAD_START_ROUTINE>(static_cast<std::uintptr_t>(loadLibrary)),
                                      job.m_remotePath, 0, nullptr);
    if (!job.m_thread)
        return failed(format(L"CreateRemoteThread не удался (код %lu)", GetLastError()));

    job.m_startTick = GetTickCount();
    return {Step::Pending, {}};
}

Result poll(Job &job, unsigned long timeoutMs)
{
    if (!job.m_thread)
        return failed(L"загрузка не начата");

    if (WaitForSingleObject(job.m_thread, 0) != WAIT_OBJECT_0) {
        if (GetTickCount() - job.m_startTick < timeoutMs)
            return {Step::Pending, {}};
        return failed(format(L"LoadLibraryW в цели не завершился за %lu с", timeoutMs / 1000));
    }

    // Код выхода потока — HMODULE загруженной DLL (32-битный), 0 — не загрузилась.
    DWORD exitCode = 0;
    GetExitCodeThread(job.m_thread, &exitCode);
    if (exitCode == 0)
        return failed(L"LoadLibraryW вернул NULL — DLL не загрузилась (разрядность/зависимости)");
    return {Step::Loaded,
            format(L"%ls загружена в процесс %lu (база 0x%08lX)", job.m_dllName.c_str(), job.m_pid, exitCode)};
}

} // namespace remote_load
