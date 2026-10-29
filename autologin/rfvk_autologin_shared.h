#pragma once
// Общая память лаунчера и rfvk_autologin.dll (автовход в клиенте).
//
// Лаунчер (x64) создаёт именованное отображение «Local\RFVK.AutoLogin.<pid>»
// до подключения библиотеки к клиенту (x86) и кладёт туда настройки входа;
// библиотека пишет обратно ход автовхода и список персонажей. Разрядности
// разные, поэтому в структуре только типы фиксированного размера и никаких
// указателей. Строки — UTF-16 (wchar_t в Windows — 2 байта у обеих сторон).

#include <cstdint>

namespace rfvk_autologin {

constexpr std::uint32_t kMagic = 0x4C414652; // 'RFAL'
constexpr std::uint32_t kVersion = 1;

// Имя отображения: kMappingPrefix + десятичный PID клиента.
constexpr wchar_t kMappingPrefix[] = L"Local\\RFVK.AutoLogin.";

// Какие шаги выполнять (лаунчер → библиотека).
enum Flags : std::uint32_t {
    SelectServer = 1u << 0, // окно «Перемещение»: кнопка сервера worldIndex
    PressStart = 1u << 1,   // лобби: «Начать игру»
    EnterFg = 1u << 2,      // окно FG-пароля: ввести fg
    SelectChar = 1u << 3,   // выбор персонажа: charName / charSlot и вход
};

// Состояние библиотеки (библиотека → лаунчер).
enum DllState : std::int32_t {
    NotAttached = 0, // библиотека ещё не открыла память
    Running = 1,     // автовход идёт
    Finished = 2,    // все шаги выполнены (или дальше — дело игрока)
    Failed = 3,      // клиент не опознан или автовход прерван
};

constexpr int kSlots = 3;      // персонажей на аккаунте
constexpr int kNameLen = 32;   // имя персонажа, с завершающим нулём
constexpr int kFgLen = 32;     // FG-пароль, ASCII, с завершающим нулём
constexpr int kEvents = 16;    // кольцо строк журнала
constexpr int kEventLen = 124; // строка журнала, с завершающим нулём

struct CharInfo {
    std::uint32_t used;  // 1 — в слоте есть персонаж
    std::uint32_t level;
    std::uint32_t race;  // раса × 2 + пол, как у клиента (0..4)
    wchar_t name[kNameLen];
};

struct Event {
    std::uint32_t level; // 0 — обычная, 1 — успех, 2 — ошибка (как журнал лаунчера)
    wchar_t text[kEventLen];
};

#pragma pack(push, 4)
struct Shared {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t size; // sizeof(Shared) — сверка обеих сторон

    // ── лаунчер → библиотека ──
    std::uint32_t flags;      // Flags
    std::uint32_t worldIndex; // номер строки в списке серверов (0 — «Сервер 1»)
    std::int32_t charSlot;    // слот 0..2 или −1; запасной вариант, если имени нет
    wchar_t charName[kNameLen]; // кого выбрать; пусто — не выбирать
    char fg[kFgLen];          // библиотека затирает сразу после чтения

    // ── библиотека → лаунчер ──
    volatile std::int32_t dllState;   // DllState
    volatile std::int32_t charsReady; // 1 — chars заполнен
    CharInfo chars[kSlots];
    volatile std::int32_t eventCount; // всего записано строк (растёт); строка i — events[i % kEvents]
    Event events[kEvents];

    // ── лаунчер → библиотека ──
    volatile std::int32_t stop; // 1 — прекратить автовход (лаунчер закрывается и т. п.)
};
#pragma pack(pop)

static_assert(sizeof(wchar_t) == 2, "UTF-16 wchar_t ожидается у обеих сторон");

} // namespace rfvk_autologin
