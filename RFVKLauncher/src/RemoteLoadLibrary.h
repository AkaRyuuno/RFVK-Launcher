#pragma once
// Загрузка DLL в 32-битный процесс клиента прямо из 64-битного лаунчера.
//
// Классический LoadLibrary-инжект: путь к DLL пишется в память цели, затем
// CreateRemoteThread запускает в ней LoadLibraryW. Адрес LoadLibraryW берётся
// не из своего kernel32 (он 64-битный), а из 32-битного kernel32.dll цели:
// модуль ищется снимком Toolhelp, адрес — по его таблице экспорта, прочитанной
// из памяти цели. Поток с 32-битным адресом начала WOW64 запускает в 32-битном
// режиме, так что отдельный x86-помощник не нужен.
//
// Чистый Win32, без Qt. Ожидание потока не блокирует: start() запускает
// загрузку, poll() проверяет, завершилась ли она.

#include <memory>
#include <string>

namespace remote_load {

enum class Step {
    Pending, // поток LoadLibraryW ещё работает — звать poll() позже
    Loaded,  // DLL загружена (или уже была загружена раньше)
    Failed,  // не загружена; причина — в message
};

struct Result {
    Step step = Step::Failed;
    std::wstring message; // строка для журнала лаунчера
};

// Незавершённая загрузка. Деструктор закрывает дескрипторы; память с путём в
// цели освобождает, только если поток уже завершился (иначе он ещё может её
// читать — оставляем, это десятки байт).
class Job {
public:
    Job() = default;
    ~Job();
    Job(const Job &) = delete;
    Job &operator=(const Job &) = delete;

private:
    friend Result start(unsigned long pid, const std::wstring &dllPath, Job &job);
    friend Result poll(Job &job, unsigned long timeoutMs);
    void *m_process = nullptr;
    void *m_thread = nullptr;
    void *m_remotePath = nullptr;
    unsigned long m_startTick = 0;
    unsigned long m_pid = 0;
    std::wstring m_dllName;
};

// Начать загрузку dllPath (абсолютный путь) в процесс pid. Сразу Loaded —
// если DLL с таким именем файла уже есть в процессе (повторный LoadLibrary
// лишь увеличил бы счётчик ссылок, DllMain не вызвался бы).
Result start(unsigned long pid, const std::wstring &dllPath, Job &job);

// Проверить загрузку, начатую start(). Дольше timeoutMs (с момента start) —
// Failed.
Result poll(Job &job, unsigned long timeoutMs);

} // namespace remote_load
