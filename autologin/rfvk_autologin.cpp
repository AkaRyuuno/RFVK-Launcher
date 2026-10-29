// rfvk_autologin.dll — автовход в клиенте RF Online (VK Play).
//
// Лаунчер подключает библиотеку к только что запущенному RF_Online.exe так же,
// как HUD (LoadLibrary-инжект, RFVKLauncher/src/RemoteLoadLibrary.*). Библиотека проходит экраны входа за игрока:
//
//   1. «Перемещение» → кнопка «Сервер 1»;
//   2. лобби → «Начать игру»;
//   3. окно FG-пароля → пароль из настроек аккаунта;
//   4. выбор персонажа → список персонажей уходит лаунчеру; если игрок выбрал
//      персонажа в лаунчере — переключиться на него и войти.
//
// Код клиента не патчится и не перехватывается: каждый шаг — вызов той же
// функции клиента, которую зовёт обработчик кнопки, с теми же проверками, что
// у клиента. Функции и глобалы ищутся сигнатурами по коду клиента. Звать их
// можно только из главного потока игры, поэтому шаги выполняются из
// системного хука очереди сообщений (WH_GETMESSAGE) на поток окна игры, как
// это делает seirin; рабочий поток библиотеки лишь будит очередь.
//
// Разбор экранов входа — по дампу newrfo-vk (адреса в комментариях — «в дампе»):
//
//   CGP_LogIn (стадия входа, vtable 0x00AB5074) держит окна глобалами:
//     CGUIMapMoveWindow 0x01021138 — «Перемещение»; в режиме списка серверов
//       (+0x18FC != 0, +0x1900 == 0) кнопка i зовёт SelectWorld(id[i]).
//     CGUILobbyMenu 0x0101A9A0 — «Начать игру» = 0x007BB650.
//     CUILockBoard 0x0101A3B0 — FG; окно CGUILockLogin по +0x168,
//       «ОК» = CUILockBoard::Certify 0x004F0780 (пароль из поля окна).
//     CGUISelectCharacter — стрелки 0x007BB930 / 0x007BB890, «Начать» 0x007BBC30.
//   Состояние экрана входа — 0x0101A330 (0x13 — FG, 0x0C — выбор персонажа),
//   выбранный слот — 0x0101A35C, занятость слотов — 0x0101A350[3],
//   «камера ещё летит» — 0x0101A34C, персонажи слотов — *(0x0102FC60) + слот × 0x61A0.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "rfvk_autologin_shared.h"

namespace al = rfvk_autologin;

namespace {

// ─────────────────────────── поиск по коду клиента ──────────────────────────

struct Pattern {
    std::uint8_t bytes[96];
    bool any[96];
    int len = 0;
};

// «55 8B ?? E8» → байты и маска.
Pattern parse(const char *text)
{
    Pattern p;
    const char *s = text;
    while (*s && p.len < 96) {
        while (*s == ' ')
            ++s;
        if (!*s)
            break;
        if (s[0] == '?') {
            p.any[p.len] = true;
            p.bytes[p.len] = 0;
            s += (s[1] == '?') ? 2 : 1;
        } else {
            unsigned v = 0;
            sscanf_s(s, "%2x", &v);
            p.any[p.len] = false;
            p.bytes[p.len] = static_cast<std::uint8_t>(v);
            s += 2;
        }
        ++p.len;
    }
    return p;
}

bool matchAt(const std::uint8_t *at, const Pattern &p)
{
    for (int i = 0; i < p.len; ++i)
        if (!p.any[i] && at[i] != p.bytes[i])
            return false;
    return true;
}

bool readable(DWORD protect)
{
    if (protect & (PAGE_GUARD | PAGE_NOACCESS))
        return false;
    return (protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ
                       | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

// Ищет шаблон в [begin, end) по доступным для чтения страницам. Возвращает
// число совпадений (до двух) и первое из них — сигнатура обязана быть уникальной.
int scanRange(std::uintptr_t begin, std::uintptr_t end, const Pattern &p, std::uintptr_t *first)
{
    int found = 0;
    std::uintptr_t a = begin;
    while (a < end && found < 2) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<void *>(a), &mbi, sizeof(mbi)))
            break;
        const auto rBegin = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const std::uintptr_t rEnd = rBegin + mbi.RegionSize;
        if (mbi.State == MEM_COMMIT && readable(mbi.Protect)) {
            const std::uintptr_t from = a > rBegin ? a : rBegin;
            const std::uintptr_t to = (rEnd < end ? rEnd : end);
            for (std::uintptr_t x = from; x + p.len <= to && found < 2; ++x) {
                if (matchAt(reinterpret_cast<const std::uint8_t *>(x), p)) {
                    if (found == 0)
                        *first = x;
                    ++found;
                }
            }
        }
        a = rEnd;
    }
    return found;
}

std::uint32_t u32At(std::uintptr_t a) { return *reinterpret_cast<const std::uint32_t *>(a); }

// ─────────────────────────── функции и данные клиента ───────────────────────

// Сигнатуры сверены по образу из дампа newrfo-vk: у каждой ровно одно совпадение.
// Абсолютные адреса и rel32 — масками; глобалы читаются операндами из кода.

// SelectWorld(u8 номер мира), cdecl: запрос выбора мира сетевому менеджеру
// и закрытие окна «Перемещение» (`mov ecx, окно` по +0x1B). В дампе — 0x0053D0B0.
constexpr const char *kSigSelectWorld =
    "55 8B EC 83 EC 40 53 56 57 66 0F B6 45 08 50 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? "
    "B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5F 5E 5B 8B E5 5D C3";
// «Начать игру» лобби, cdecl без аргументов. Окно лобби по +0x20, состояние
// экрана входа по +0x2B (`mov [состояние], 0x13`), CUILockBoard по +0x34.
// В дампе — 0x007BB650.
constexpr const char *kSigLobbyStart =
    "55 8B EC 83 EC 40 53 56 57 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 7E 07 E8 ?? ?? ?? ?? "
    "EB 1E B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? 13 00 00 00 B9 ?? ?? ?? ?? E8";
// Стрелки выбора персонажа, cdecl без аргументов: «следующий» 0→1→2→0
// (в дампе 0x007BB930) и «предыдущий» (0x007BB890). «Камера летит» по +0x14,
// выбранный слот по +0x1D.
constexpr const char *kSigCharNext =
    "55 8B EC 83 EC 44 53 56 57 83 3D ?? ?? ?? ?? 0C 75 78 C7 05 ?? ?? ?? ?? 01 00 00 00 "
    "A1 ?? ?? ?? ?? 89 45 BC 83 7D BC 00 74 0E 83 7D BC 01 74 20 83 7D BC 02 74 32 EB 46 "
    "C7 05 ?? ?? ?? ?? 01 00 00 00 6A 00 6A 03";
constexpr const char *kSigCharPrev =
    "55 8B EC 83 EC 44 53 56 57 83 3D ?? ?? ?? ?? 0C 75 78 C7 05 ?? ?? ?? ?? 01 00 00 00 "
    "A1 ?? ?? ?? ?? 89 45 BC 83 7D BC 00 74 0E 83 7D BC 01 74 20 83 7D BC 02 74 32 EB 46 "
    "C7 05 ?? ?? ?? ?? 02 00 00 00 6A 00 6A 08";
// «Начать» выбора персонажа, cdecl без аргументов: входит выбранным слотом.
// Занятость слотов по +0x1E. В дампе — 0x007BBC30.
constexpr const char *kSigCharStart =
    "55 8B EC 83 EC 48 53 56 57 83 3D ?? ?? ?? ?? 0C 0F 85 ?? ?? ?? ?? A1 ?? ?? ?? ?? "
    "83 3C 85 ?? ?? ?? ?? 00 75 30 6A 01";
// Внутри «Начать»: `imul ecx, ecx, шаг; add ecx, [массив персонажей]`.
constexpr const char *kSigSlotArray = "69 C9 ?? ?? 00 00 03 0D";
// CUILockBoard::Certify(const char* пароль), __thiscall, ret 4: если клиент не
// ждёт ответа и пароль не пуст — запрос проверки FG. В дампе — 0x004F0780.
constexpr const char *kSigCertify =
    "55 8B EC 83 EC 44 53 56 57 89 4D FC B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 74 02 EB 27 "
    "83 7D 08 00 74 10 8B 45 08 50 E8 ?? ?? ?? ?? 83 C4 04 85 C0 75 02 EB 0F 8B 45 08 50 "
    "8B 4D FC 8B 49 0C E8";

// Поля объектов (код без оптимизации, смещения стабильны между патчами;
// сверены по дампу и по коду обработчиков).
constexpr std::uintptr_t kWndShown = 0x30;        // CGUIObject: окно показано (геттер 0x00407D50)
constexpr std::uintptr_t kMmCount = 0x3A4;        // CGUIMapMoveWindow: u8 число строк
constexpr std::uintptr_t kMmIds = 0x152C;         // u32* номера (младший байт — номер мира)
constexpr std::uintptr_t kMmServerList = 0x18FC;  // != 0 — окно в режиме списка серверов
constexpr std::uintptr_t kMmMode = 0x1900;        // 0 — выбор (кнопка зовёт SelectWorld)
constexpr std::uintptr_t kLockLoginWnd = 0x168;   // CUILockBoard: CGUILockLogin* (+0x10 → +0x158)
constexpr std::uintptr_t kLockHasPw = 0x174;      // u8 != 0 — FG задан (окно входа, не создания)
constexpr std::uintptr_t kLockResetSlot = 0x9C;   // vtable CGUILockLogin: очистить поле ввода
constexpr std::uintptr_t kCharName = 0x10;        // персонаж: имя, cp1251 (геттер 0x004030D0)
constexpr std::uintptr_t kCharRace = 0x458C;      // u8 раса × 2 + пол (0x00414430)
constexpr std::uintptr_t kCharLevel = 0x4590;     // u32 уровень (0x0041C630)
constexpr std::uint32_t kStateCharSelect = 0x0C;  // состояние «выбор персонажа»
constexpr std::uint32_t kStateCharWait = 0x0F;    // ждём ответа на выбор персонажа («Начать» 0x007BBC30)
constexpr std::uint32_t kStateEnterWorld = 0x14;  // вход в мир (тот же 0x007BBC30, ответы сервера)

using VoidFn = void(__cdecl *)();
using SelectWorldFn = void(__cdecl *)(std::uint32_t world);
using CertifyFn = void(__fastcall *)(void *board, void *edx, const char *password);
using ResetFn = void(__fastcall *)(void *self, void *edx);

struct Client {
    SelectWorldFn selectWorld = nullptr;
    VoidFn lobbyStart = nullptr;
    VoidFn charNext = nullptr;
    VoidFn charPrev = nullptr;
    VoidFn charStart = nullptr;
    CertifyFn certify = nullptr;

    std::uintptr_t mapMove = 0;    // объект окна «Перемещение»
    std::uintptr_t lobby = 0;      // объект лобби
    std::uintptr_t lockBoard = 0;  // CUILockBoard
    std::uintptr_t state = 0;      // u32 состояние экрана входа
    std::uintptr_t camMoving = 0;  // u32 камера ещё летит
    std::uintptr_t slot = 0;       // u32 выбранный слот
    std::uintptr_t occupied = 0;   // u32[3] занятость слотов
    std::uintptr_t slotArray = 0;  // указатель на массив персонажей слотов
    std::uint32_t slotStride = 0;  // размер персонажа слота
};

Client g_client;

// ─────────────────────────── общая память и журнал ──────────────────────────

HMODULE g_self = nullptr;
HANDLE g_mapping = nullptr;
al::Shared *g_shared = nullptr;

void say(int level, const wchar_t *fmt, ...)
{
    if (!g_shared)
        return;
    wchar_t buf[al::kEventLen];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);

    const LONG n = g_shared->eventCount;
    al::Event &e = g_shared->events[n % al::kEvents];
    e.level = static_cast<std::uint32_t>(level);
    wcscpy_s(e.text, buf);
    MemoryBarrier();
    InterlockedExchange(reinterpret_cast<volatile LONG *>(&g_shared->eventCount), n + 1);
}

void setState(al::DllState s)
{
    InterlockedExchange(reinterpret_cast<volatile LONG *>(&g_shared->dllState), s);
}

bool resolveClient(bool verbose)
{
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    const std::uintptr_t end = base + nt->OptionalHeader.SizeOfImage;

    struct Want { const char *name; const char *sig; std::uintptr_t *out; };
    std::uintptr_t selectWorld = 0, lobbyStart = 0, charNext = 0, charPrev = 0, charStart = 0, certify = 0;
    const Want wants[] = {
        {"SelectWorld", kSigSelectWorld, &selectWorld},
        {"LobbyStart", kSigLobbyStart, &lobbyStart},
        {"CharNext", kSigCharNext, &charNext},
        {"CharPrev", kSigCharPrev, &charPrev},
        {"CharStart", kSigCharStart, &charStart},
        {"Certify", kSigCertify, &certify},
    };
    for (const Want &w : wants) {
        std::uintptr_t at = 0;
        const int n = scanRange(base, end, parse(w.sig), &at);
        if (n != 1) {
            if (verbose)
                say(2, L"Автовход: клиент не опознан (%hs: совпадений %d)", w.name, n);
            return false;
        }
        *w.out = at;
    }

    std::uintptr_t slotCode = 0;
    if (scanRange(charStart, charStart + 0x400, parse(kSigSlotArray), &slotCode) < 1) {
        if (verbose)
            say(2, L"Автовход: клиент не опознан (массив персонажей)");
        return false;
    }

    Client c;
    c.selectWorld = reinterpret_cast<SelectWorldFn>(selectWorld);
    c.lobbyStart = reinterpret_cast<VoidFn>(lobbyStart);
    c.charNext = reinterpret_cast<VoidFn>(charNext);
    c.charPrev = reinterpret_cast<VoidFn>(charPrev);
    c.charStart = reinterpret_cast<VoidFn>(charStart);
    c.certify = reinterpret_cast<CertifyFn>(certify);
    c.mapMove = u32At(selectWorld + 0x1B);
    c.lobby = u32At(lobbyStart + 0x20);
    c.state = u32At(lobbyStart + 0x2B);
    c.lockBoard = u32At(lobbyStart + 0x34);
    c.camMoving = u32At(charNext + 0x14);
    c.slot = u32At(charNext + 0x1D);
    c.occupied = u32At(charStart + 0x1E);
    c.slotStride = u32At(slotCode + 2);
    c.slotArray = u32At(slotCode + 8);

    // Те же глобалы, прочитанные из разных функций, обязаны совпасть.
    if (u32At(charNext + 0x0B) != c.state || u32At(charStart + 0x0B) != c.state
        || u32At(charStart + 0x17) != c.slot || u32At(charPrev + 0x1D) != c.slot
        || c.slotStride < 0x100 || c.slotStride > 0x100000) {
        if (verbose)
            say(2, L"Автовход: клиент не опознан (глобалы экрана входа не сходятся)");
        return false;
    }
    g_client = c;
    return true;
}

// ─────────────────────────── автомат входа ──────────────────────────────────

enum class Step { Server, Lobby, Fg, Chars, Done };

struct Auto {
    Step step = Step::Server;
    std::uint32_t flags = 0;
    std::uint32_t worldIndex = 0;
    std::int32_t wantSlot = -1;
    wchar_t wantName[al::kNameLen] = {};
    char fg[al::kFgLen] = {};

    DWORD seenAt = 0;       // когда текущее окно впервые увидели
    DWORD fgSentAt = 0;     // когда отправили FG (0 — не отправляли)
    bool fgWarned = false;
    bool charsSent = false;
    int arrowPresses = 0;
    DWORD lastTick = 0;
};

Auto g_auto;
volatile LONG g_finished = 0; // автомат закончил — рабочему потоку пора выгружаться
volatile LONG g_inTick = 0;   // тик идёт прямо сейчас (не выгружаться под ним)

constexpr DWORD kSettleMs = 700;     // пауза после появления окна — как у человека
constexpr DWORD kFgRejectMs = 6000;  // окно FG не закрылось за это время — пароль не принят
constexpr int kMaxArrowPresses = 6;

bool shown(std::uintptr_t wnd) { return wnd && u32At(wnd + kWndShown) != 0; }

// Окно появилось и постояло kSettleMs: клиент успел его дорисовать и заполнить.
bool settled(bool visible, DWORD now)
{
    if (!visible) {
        g_auto.seenAt = 0;
        return false;
    }
    if (!g_auto.seenAt)
        g_auto.seenAt = now ? now : 1;
    return now - g_auto.seenAt >= kSettleMs;
}

void finish(const wchar_t *why, int level)
{
    if (why)
        say(level, L"%ls", why);
    g_auto.step = Step::Done;
    SecureZeroMemory(g_auto.fg, sizeof(g_auto.fg));
    setState(al::Finished);
    InterlockedExchange(&g_finished, 1);
}

std::uintptr_t lockLoginWnd()
{
    return g_client.lockBoard ? u32At(g_client.lockBoard + kLockLoginWnd) : 0;
}

void readName(std::uintptr_t ch, wchar_t (&out)[al::kNameLen])
{
    char raw[al::kNameLen] = {};
    memcpy(raw, reinterpret_cast<const void *>(ch + kCharName), al::kNameLen - 1);
    raw[al::kNameLen - 1] = 0;
    out[0] = 0;
    // Имена клиента — в кодировке cp1251 (русский клиент VK Play).
    MultiByteToWideChar(1251, 0, raw, -1, out, al::kNameLen);
    out[al::kNameLen - 1] = 0;
}

// Персонажи слотов → лаунчеру (один раз за запуск).
void publishChars(std::uintptr_t array)
{
    al::CharInfo list[al::kSlots] = {};
    int count = 0;
    for (int i = 0; i < al::kSlots; ++i) {
        if (!u32At(g_client.occupied + 4 * i))
            continue;
        const std::uintptr_t ch = array + static_cast<std::uintptr_t>(i) * g_client.slotStride;
        list[i].used = 1;
        list[i].level = u32At(ch + kCharLevel);
        list[i].race = *reinterpret_cast<const std::uint8_t *>(ch + kCharRace);
        readName(ch, list[i].name);
        ++count;
    }
    memcpy(g_shared->chars, list, sizeof(list));
    MemoryBarrier();
    InterlockedExchange(reinterpret_cast<volatile LONG *>(&g_shared->charsReady), 1);
    say(0, L"Автовход: персонажей на аккаунте — %d", count);
}

// Слот нужного персонажа: по имени, иначе — сохранённый номер слота, если имени нет.
int targetSlot(std::uintptr_t array)
{
    if (g_auto.wantName[0]) {
        for (int i = 0; i < al::kSlots; ++i) {
            if (!u32At(g_client.occupied + 4 * i))
                continue;
            wchar_t name[al::kNameLen];
            readName(array + static_cast<std::uintptr_t>(i) * g_client.slotStride, name);
            if (wcscmp(name, g_auto.wantName) == 0)
                return i;
        }
        return -1;
    }
    const int s = g_auto.wantSlot;
    if (s >= 0 && s < al::kSlots && u32At(g_client.occupied + 4 * s))
        return s;
    return -1;
}

void tick()
{
    const DWORD now = GetTickCount();
    if (now - g_auto.lastTick < 100)
        return;
    g_auto.lastTick = now;

    if (g_shared->stop) {
        finish(L"Автовход: остановлен лаунчером", 0);
        return;
    }

    const Client &c = g_client;
    const std::uint32_t state = u32At(c.state);

    // Персонаж уже выбран (игрок успел сам) — экран входа пройден. Дальше
    // ничего не трогаем: в игре окно «Перемещение» — это телепорт.
    if (state == kStateCharWait || state == kStateEnterWorld) {
        const std::uintptr_t array = u32At(c.slotArray);
        if (!g_auto.charsSent && array) {
            g_auto.charsSent = true;
            publishChars(array);
        }
        finish(L"Автовход: персонаж выбран — вход в игру", 1);
        return;
    }

    // Экран выбора персонажа наступил раньше ожидаемого шага (игрок прошёл
    // предыдущие окна сам или FG не нужен) — сразу к персонажам.
    if (g_auto.step != Step::Chars && g_auto.step != Step::Done && state == kStateCharSelect
        && !shown(c.lobby) && !shown(c.mapMove) && !shown(lockLoginWnd())) {
        g_auto.step = Step::Chars;
        g_auto.seenAt = 0;
    }

    switch (g_auto.step) {
    case Step::Server: {
        const std::uintptr_t mm = c.mapMove;
        const bool ready = shown(mm) && u32At(mm + kMmServerList) != 0 && u32At(mm + kMmMode) == 0
                           && *reinterpret_cast<const std::uint8_t *>(mm + kMmCount) > 0;
        if (shown(c.lobby)) { // список серверов уже прошли
            g_auto.step = Step::Lobby;
            g_auto.seenAt = 0;
            break;
        }
        if (!(g_auto.flags & al::SelectServer)) {
            g_auto.step = Step::Lobby;
            break;
        }
        if (!settled(ready, now))
            break;
        const unsigned count = *reinterpret_cast<const std::uint8_t *>(mm + kMmCount);
        const unsigned row = g_auto.worldIndex < count ? g_auto.worldIndex : 0;
        const auto *ids = reinterpret_cast<const std::uint32_t *>(u32At(mm + kMmIds));
        if (!ids)
            break;
        const std::uint32_t world = ids[row] & 0xFF; // как обработчик кнопки: младший байт
        say(0, L"Автовход: выбираю сервер %u (строка %u из %u)", world + 1, row + 1, count);
        c.selectWorld(world);
        g_auto.step = Step::Lobby;
        g_auto.seenAt = 0;
        break;
    }
    case Step::Lobby:
        if (!(g_auto.flags & al::PressStart)) {
            g_auto.step = Step::Fg;
            break;
        }
        if (!settled(shown(c.lobby), now))
            break;
        say(0, L"Автовход: «Начать игру»");
        c.lobbyStart();
        g_auto.step = Step::Fg;
        g_auto.seenAt = 0;
        break;

    case Step::Fg: {
        const std::uintptr_t wnd = lockLoginWnd();
        const bool visible = shown(wnd);
        if (g_auto.fgSentAt) {
            // Окно FG так и не закрылось — пароль не принят. Дальше — игрок сам.
            if (visible && !g_auto.fgWarned && now - g_auto.fgSentAt > kFgRejectMs) {
                g_auto.fgWarned = true;
                say(2, L"Автовход: FG-пароль не принят — введите его вручную и проверьте в лаунчере");
            }
            break; // ждём экран персонажей (переход — в начале тика)
        }
        if (!(g_auto.flags & al::EnterFg) || !g_auto.fg[0])
            break; // FG вводит игрок; ждём экран персонажей
        if (!settled(visible, now))
            break;
        if (*reinterpret_cast<const std::uint8_t *>(c.lockBoard + kLockHasPw) == 0) {
            say(2, L"Автовход: на аккаунте не задан FG-пароль — клиент просит создать новый");
            g_auto.fgSentAt = now;
            g_auto.fgWarned = true;
            break;
        }
        say(0, L"Автовход: ввожу FG-пароль");
        c.certify(reinterpret_cast<void *>(c.lockBoard), nullptr, g_auto.fg);
        // Как обработчик «ОК» клиента: после запроса очистить поле окна.
        auto **vtable = *reinterpret_cast<void ***>(wnd);
        reinterpret_cast<ResetFn>(vtable[kLockResetSlot / sizeof(void *)])(reinterpret_cast<void *>(wnd), nullptr);
        SecureZeroMemory(g_auto.fg, sizeof(g_auto.fg));
        g_auto.fgSentAt = now ? now : 1;
        break;
    }
    case Step::Chars: {
        if (state != kStateCharSelect)
            break;
        if (u32At(c.camMoving) != 0) { // камера переезжает к другому слоту
            g_auto.seenAt = 0;
            break;
        }
        const std::uintptr_t array = u32At(c.slotArray);
        if (!settled(array != 0, now))
            break;
        if (!g_auto.charsSent) {
            g_auto.charsSent = true;
            publishChars(array);
        }
        if (!(g_auto.flags & al::SelectChar) || (!g_auto.wantName[0] && g_auto.wantSlot < 0)) {
            finish(L"Автовход: персонажа выбирает игрок", 1);
            break;
        }
        const int target = targetSlot(array);
        if (target < 0) {
            finish(L"Автовход: выбранный в лаунчере персонаж не найден на аккаунте — выберите вручную", 2);
            break;
        }
        const int cur = static_cast<int>(u32At(c.slot));
        if (cur != target) {
            if (g_auto.arrowPresses++ >= kMaxArrowPresses) {
                finish(L"Автовход: не удалось переключиться на персонажа — выберите вручную", 2);
                break;
            }
            // Стрелки ходят по кругу 0→1→2: в нужную сторону за одно нажатие.
            if (target == (cur + 1) % al::kSlots)
                c.charNext();
            else
                c.charPrev();
            g_auto.seenAt = 0;
            break;
        }
        wchar_t name[al::kNameLen];
        readName(array + static_cast<std::uintptr_t>(target) * c.slotStride, name);
        say(0, L"Автовход: вхожу персонажем %ls", name);
        c.charStart();
        finish(L"Автовход: готово", 1);
        break;
    }
    case Step::Done:
        break;
    }
}

// Тик под защитой: любой сбой чтения памяти клиента — прекратить автовход,
// а не уронить игру. Без C++-объектов с деструкторами (C2712).
void guardedTick()
{
    __try {
        tick();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_auto.step = Step::Done;
        say(2, L"Автовход: сбой при обращении к клиенту — прекращаю, дальше вручную");
        setState(al::Failed);
        InterlockedExchange(&g_finished, 1);
    }
}

// ─────────────────────────── главный поток игры ─────────────────────────────

HHOOK g_hook = nullptr;

LRESULT CALLBACK getMessageHook(int code, WPARAM remove, LPARAM msg)
{
    if (code == HC_ACTION && remove == PM_REMOVE && !g_finished) {
        InterlockedExchange(&g_inTick, 1);
        if (!g_finished)
            guardedTick();
        InterlockedExchange(&g_inTick, 0);
    }
    return CallNextHookEx(nullptr, code, remove, msg);
}

struct WindowSearch {
    DWORD pid = 0;
    std::uintptr_t imageBegin = 0;
    std::uintptr_t imageEnd = 0;
    HWND found = nullptr;
};

// Окно игры: видимое окно верхнего уровня нашего процесса без владельца,
// оконная процедура класса которого — в коде клиента (как в seirin: свои окна
// в процессе заводят и чужие модули).
BOOL CALLBACK matchGameWindow(HWND hwnd, LPARAM param)
{
    auto *s = reinterpret_cast<WindowSearch *>(param);
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER))
        return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != s->pid)
        return TRUE;
    const auto inImage = [s](ULONG_PTR a) { return a >= s->imageBegin && a < s->imageEnd; };
    if (!inImage(GetClassLongPtrA(hwnd, GCLP_WNDPROC)) && !inImage(GetClassLongPtrW(hwnd, GCLP_WNDPROC)))
        return TRUE;
    s->found = hwnd;
    return FALSE;
}

HWND findGameWindow()
{
    WindowSearch s;
    s.pid = GetCurrentProcessId();
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    s.imageBegin = base;
    s.imageEnd = base + nt->OptionalHeader.SizeOfImage;
    EnumWindows(matchGameWindow, reinterpret_cast<LPARAM>(&s));
    return s.found;
}

// ─────────────────────────── рабочий поток ──────────────────────────────────

constexpr DWORD kWaitMappingMs = 10000;   // лаунчер создаёт память до инжекта
constexpr DWORD kWaitClientMs = 60000;    // код клиента может распаковываться не сразу
constexpr DWORD kAutoLoginTimeoutMs = 15 * 60 * 1000; // дальше — точно не экран входа

bool openShared()
{
    wchar_t name[64];
    swprintf_s(name, L"%ls%lu", al::kMappingPrefix, GetCurrentProcessId());
    const DWORD start = GetTickCount();
    while (!g_mapping && GetTickCount() - start < kWaitMappingMs) {
        g_mapping = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name);
        if (!g_mapping)
            Sleep(200);
    }
    if (!g_mapping)
        return false;
    g_shared = static_cast<al::Shared *>(MapViewOfFile(g_mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(al::Shared)));
    if (!g_shared || g_shared->magic != al::kMagic || g_shared->version != al::kVersion
        || g_shared->size != sizeof(al::Shared)) {
        if (g_shared)
            UnmapViewOfFile(g_shared);
        g_shared = nullptr;
        return false;
    }
    return true;
}

void closeShared()
{
    if (g_shared)
        UnmapViewOfFile(g_shared);
    if (g_mapping)
        CloseHandle(g_mapping);
    g_shared = nullptr;
    g_mapping = nullptr;
}

DWORD WINAPI worker(LPVOID)
{
    if (!openShared()) {
        closeShared();
        FreeLibraryAndExitThread(g_self, 0);
    }

    // Настройки входа — себе; пароль в общей памяти сразу затираем.
    g_auto.flags = g_shared->flags;
    g_auto.worldIndex = g_shared->worldIndex;
    g_auto.wantSlot = g_shared->charSlot;
    memcpy(g_auto.wantName, g_shared->charName, sizeof(g_auto.wantName));
    g_auto.wantName[al::kNameLen - 1] = 0;
    memcpy(g_auto.fg, g_shared->fg, sizeof(g_auto.fg));
    g_auto.fg[al::kFgLen - 1] = 0;
    SecureZeroMemory(const_cast<char *>(g_shared->fg), sizeof(g_shared->fg));
    setState(al::Running);

    // Клиент: сигнатуры (код может быть ещё не распакован — повторяем) и окно.
    bool ok = false;
    HWND wnd = nullptr;
    const DWORD start = GetTickCount();
    while (GetTickCount() - start < kWaitClientMs && !g_shared->stop) {
        if (!ok)
            ok = resolveClient(false);
        if (ok && !wnd)
            wnd = findGameWindow();
        if (ok && wnd)
            break;
        Sleep(1000);
    }
    if (!ok || !wnd) {
        if (!ok)
            resolveClient(true); // ещё раз — уже с причиной в журнал
        else
            say(2, L"Автовход: окно игры не найдено");
        SecureZeroMemory(g_auto.fg, sizeof(g_auto.fg));
        setState(al::Failed);
        closeShared();
        FreeLibraryAndExitThread(g_self, 0);
    }

    const DWORD gameThread = GetWindowThreadProcessId(wnd, nullptr);
    g_hook = SetWindowsHookExW(WH_GETMESSAGE, getMessageHook, nullptr, gameThread);
    if (!g_hook) {
        say(2, L"Автовход: не удалось подключиться к очереди окна (ошибка %lu)", GetLastError());
        SecureZeroMemory(g_auto.fg, sizeof(g_auto.fg));
        setState(al::Failed);
        closeShared();
        FreeLibraryAndExitThread(g_self, 0);
    }
    say(0, L"Автовход: подключён к клиенту, жду экран выбора сервера");

    // Будим очередь окна: тик выполняется в хуке на любом сообщении.
    const DWORD begin = GetTickCount();
    while (!g_finished) {
        if (GetTickCount() - begin > kAutoLoginTimeoutMs) {
            say(2, L"Автовход: экран входа не пройден за 15 минут — прекращаю");
            break;
        }
        if (!IsWindow(wnd))
            break;
        PostMessageW(wnd, WM_NULL, 0, 0);
        Sleep(100);
    }

    InterlockedExchange(&g_finished, 1);
    UnhookWindowsHookEx(g_hook);
    // Хук мог быть в работе на главном потоке в момент снятия — дождаться выхода.
    for (int i = 0; i < 50 && g_inTick; ++i)
        Sleep(20);
    Sleep(200);
    SecureZeroMemory(g_auto.fg, sizeof(g_auto.fg));
    if (g_shared && g_shared->dllState == al::Running)
        setState(al::Finished);
    closeShared();
    FreeLibraryAndExitThread(g_self, 0);
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = module;
        DisableThreadLibraryCalls(module);
        // Под loader lock — только завести поток; вся работа — в нём.
        HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (t)
            CloseHandle(t);
    }
    return TRUE;
}
