// rfvk_proxy.dll — направляет TCP-соединения клиента RF Online через прокси.
//
// Лаунчер подключает библиотеку к только что запущенному RF_Online.exe так же,
// как автовход (LoadLibrary-инжект, RFVKLauncher/src/RemoteLoadLibrary.*).
// Настройки прокси — в общей памяти (rfvk_proxy_shared.h).
//
// Перехваты в ws2_32.dll — hotpatch: у экспортов x86-ws2_32 пролог
// «8B FF» (mov edi, edi), перед ним 5 байт заполнителя (CC). В заполнитель
// пишется JMP на наш обработчик, затем «mov edi, edi» атомарно заменяется на
// короткий JMP назад. Оригинал вызывается с адреса функции + 2 — трамплины и
// копирование кода не нужны. Чужой пролог (функцию уже перехватил кто-то
// другой) — перехват не ставится, и лаунчер закрывает клиент.
//
// Клиент работает с неблокирующими сокетами через WSAEventSelect (импорт
// WSACreateEvent / WSAEventSelect / WSAEnumNetworkEvents). Поэтому connect()
// к игровому серверу:
//   1. снимает с сокета регистрацию событий (сокет остаётся неблокирующим);
//   2. подключается к прокси и проводит рукопожатие, ожидая готовности через
//      select() с общим дедлайном;
//   3. возвращает регистрацию и сообщает итог так, как его ждёт клиент:
//      WSAEWOULDBLOCK и FD_CONNECT (событие взводится, FD_CONNECT
//      досылается в WSAEnumNetworkEvents). Блокирующий сокет получает
//      обычный результат connect().
// После рукопожатия данные идут по сокету как есть, библиотека их не трогает.
// getpeername() отдаёт адрес игрового сервера, а не прокси.
//
// Утечки мимо прокси закрыты так: TCP по IPv6 к нелокальным адресам
// запрещён (прокси принимает только IPv4-цели), а сразу после установки
// перехватов библиотека проверяет, не открыл ли клиент соединений раньше.
// Ограничения: UDP и ConnectEx (через WSAIoctl) не перехватываются — клиент
// ими не пользуется; имена игровых серверов клиент разрешает сам.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "rfvk_proxy_shared.h"

namespace px = rfvk_proxy;

namespace {

constexpr DWORD kHandshakeTimeoutMs = 15000; // подключение к прокси + рукопожатие
constexpr int kMaxHttpHeader = 8192;         // ответ HTTP-прокси длиннее — ошибка
constexpr int kMaxSockets = 256;             // отслеживаемые сокеты клиента

// ─────────────────────────── настройки прокси ───────────────────────────────

// Копия настроек из общей памяти: читается один раз при загрузке, дальше не
// меняется — потокам не нужна синхронизация, а строки гарантированно с нулём.
struct Config {
    px::ProxyType type = px::ProxyType::Direct;
    char host[px::kHostLen] = {};
    std::uint16_t port = 0;
    bool auth = false;
    char user[px::kAuthLen] = {};
    char pass[px::kAuthLen] = {};
};

Config g_conf;
HANDLE g_map = nullptr;
px::SharedConfig *g_shared = nullptr; // для статуса; настройки — из g_conf
CRITICAL_SECTION g_statusCs;

template <size_t N>
void copyZ(char (&dst)[N], const char *src)
{
    std::memcpy(dst, src, N);
    dst[N - 1] = '\0';
}

void setStatus(px::TunnelState state, int err, const wchar_t *fmt, ...)
{
    if (!g_shared)
        return;
    EnterCriticalSection(&g_statusCs);
    g_shared->tunnelState = state;
    g_shared->lastError = err;
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(g_shared->statusText, px::kStatusLen, _TRUNCATE, fmt, ap);
    va_end(ap);
    LeaveCriticalSection(&g_statusCs);
}

void setHookStatus(px::HookStatus status, const wchar_t *text)
{
    if (!g_shared)
        return;
    EnterCriticalSection(&g_statusCs);
    wcsncpy_s(g_shared->statusText, text, _TRUNCATE);
    LeaveCriticalSection(&g_statusCs);
    InterlockedExchange(reinterpret_cast<volatile LONG *>(&g_shared->hookStatus), status);
}

bool openSharedConfig()
{
    wchar_t name[64];
    _snwprintf_s(name, _TRUNCATE, L"%ls%lu", px::kMappingPrefix, GetCurrentProcessId());
    g_map = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name);
    if (!g_map)
        return false;
    g_shared = static_cast<px::SharedConfig *>(
        MapViewOfFile(g_map, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(px::SharedConfig)));
    if (!g_shared || g_shared->magic != px::kMagic || g_shared->version != px::kVersion
        || g_shared->size != sizeof(px::SharedConfig)) {
        if (g_shared)
            UnmapViewOfFile(g_shared);
        g_shared = nullptr;
        CloseHandle(g_map);
        g_map = nullptr;
        return false;
    }
    g_conf.type = static_cast<px::ProxyType>(g_shared->type);
    copyZ(g_conf.host, g_shared->proxyHost);
    g_conf.port = g_shared->proxyPort;
    g_conf.auth = g_shared->authEnabled != 0;
    copyZ(g_conf.user, g_shared->username);
    copyZ(g_conf.pass, g_shared->password);
    // Пароль в общей памяти больше не нужен.
    SecureZeroMemory(g_shared->password, sizeof(g_shared->password));
    return true;
}

// ─────────────────────────── hotpatch-перехват ──────────────────────────────

// Ставит перехват на функцию с прологом «8B FF» и 5 байтами CC/90 перед ней.
// Возвращает адрес продолжения оригинала (функция + 2) или nullptr.
void *hotpatch(void *target, void *detour)
{
    auto *p = static_cast<std::uint8_t *>(target);
    if (!p || (reinterpret_cast<std::uintptr_t>(p) & 1) != 0) // атомарная запись 2 байт — по чётному адресу
        return nullptr;
    if (p[0] != 0x8B || p[1] != 0xFF)
        return nullptr;
    for (int i = 1; i <= 5; ++i) {
        if (p[-i] != 0xCC && p[-i] != 0x90)
            return nullptr;
    }

    DWORD old = 0;
    if (!VirtualProtect(p - 5, 7, PAGE_EXECUTE_READWRITE, &old))
        return nullptr;
    p[-5] = 0xE9; // JMP rel32 → обработчик
    const auto rel = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(detour)
                                               - reinterpret_cast<std::uintptr_t>(p));
    std::memcpy(p - 4, &rel, sizeof(rel));
    // «mov edi, edi» → «jmp $-5» (EB F9) одной атомарной записью: поток,
    // который сейчас входит в функцию, видит либо старый, либо новый пролог.
    InterlockedExchange16(reinterpret_cast<volatile SHORT *>(p), static_cast<SHORT>(0xF9EB));
    VirtualProtect(p - 5, 7, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p - 5, 7);
    return p + 2;
}

// ─────────────────────────── оригиналы ws2_32 ───────────────────────────────

using FnConnect = int(WSAAPI *)(SOCKET, const sockaddr *, int);
using FnWSAConnect = int(WSAAPI *)(SOCKET, const sockaddr *, int, LPWSABUF, LPWSABUF, LPQOS, LPQOS);
using FnGetPeerName = int(WSAAPI *)(SOCKET, sockaddr *, int *);
using FnCloseSocket = int(WSAAPI *)(SOCKET);
using FnWSAEventSelect = int(WSAAPI *)(SOCKET, WSAEVENT, long);
using FnWSAEnumNetworkEvents = int(WSAAPI *)(SOCKET, WSAEVENT, LPWSANETWORKEVENTS);

FnConnect g_connect = nullptr;
FnWSAConnect g_wsaConnect = nullptr;
FnGetPeerName g_getPeerName = nullptr;
FnCloseSocket g_closeSocket = nullptr;
FnWSAEventSelect g_eventSelect = nullptr;
FnWSAEnumNetworkEvents g_enumEvents = nullptr;

// ─────────────────────────── учёт сокетов ───────────────────────────────────

struct SockInfo {
    SOCKET s = INVALID_SOCKET;
    WSAEVENT event = nullptr;  // регистрация WSAEventSelect клиента
    long mask = 0;
    bool tunneled = false;     // соединён через прокси
    sockaddr_in peer = {};     // настоящий адрес для getpeername
    bool fdConnectDue = false; // досылать FD_CONNECT в WSAEnumNetworkEvents
    int fdConnectError = 0;
    bool connectInFlight = false; // вернули WSAEWOULDBLOCK без регистрации событий
};

SockInfo g_socks[kMaxSockets];
CRITICAL_SECTION g_sockCs;

// Под g_sockCs. create — занять свободную запись, если сокета нет.
SockInfo *findSock(SOCKET s, bool create)
{
    SockInfo *freeSlot = nullptr;
    for (auto &e : g_socks) {
        if (e.s == s)
            return &e;
        if (!freeSlot && e.s == INVALID_SOCKET)
            freeSlot = &e;
    }
    if (!create || !freeSlot)
        return nullptr;
    *freeSlot = SockInfo{};
    freeSlot->s = s;
    return freeSlot;
}

// ─────────────────────────── ввод-вывод с дедлайном ─────────────────────────

int remainingMs(DWORD deadline)
{
    const auto left = static_cast<LONG>(deadline - GetTickCount());
    return left > 0 ? static_cast<int>(left) : 0;
}

// Ждёт готовности сокета (чтение или запись). 0 — готов, иначе код ошибки.
int waitReady(SOCKET s, bool forWrite, DWORD deadline)
{
    const int ms = remainingMs(deadline);
    if (ms == 0)
        return WSAETIMEDOUT;
    fd_set rw;
    fd_set ex;
    FD_ZERO(&rw);
    FD_ZERO(&ex);
    FD_SET(s, &rw);
    FD_SET(s, &ex);
    timeval tv{ms / 1000, (ms % 1000) * 1000};
    const int n = select(0, forWrite ? nullptr : &rw, forWrite ? &rw : nullptr, &ex, &tv);
    if (n == SOCKET_ERROR)
        return WSAGetLastError();
    if (n == 0)
        return WSAETIMEDOUT;
    if (FD_ISSET(s, &ex)) {
        int err = 0;
        int len = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &len);
        return err ? err : WSAECONNREFUSED;
    }
    return 0;
}

int sendAll(SOCKET s, const void *data, int len, DWORD deadline)
{
    const auto *p = static_cast<const char *>(data);
    while (len > 0) {
        if (const int err = waitReady(s, true, deadline))
            return err;
        const int n = send(s, p, len, 0);
        if (n == SOCKET_ERROR) {
            const int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
                continue;
            return err;
        }
        p += n;
        len -= n;
    }
    return 0;
}

int recvExact(SOCKET s, void *data, int len, DWORD deadline)
{
    auto *p = static_cast<char *>(data);
    while (len > 0) {
        if (const int err = waitReady(s, false, deadline))
            return err;
        const int n = recv(s, p, len, 0);
        if (n == 0)
            return WSAECONNRESET; // прокси закрыл соединение посреди рукопожатия
        if (n == SOCKET_ERROR) {
            const int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
                continue;
            return err;
        }
        p += n;
        len -= n;
    }
    return 0;
}

// ─────────────────────────── рукопожатия ────────────────────────────────────

// Итог рукопожатия: код Winsock (0 — успех) и пояснение для журнала.
struct HsResult {
    int err = 0;
    const wchar_t *what = L"";
};

int socksReplyToWsa(std::uint8_t rep)
{
    switch (rep) {
    case 0x02: return WSAEACCES;       // запрещено правилами прокси
    case 0x03: return WSAENETUNREACH;
    case 0x04: return WSAEHOSTUNREACH;
    case 0x05: return WSAECONNREFUSED;
    case 0x06: return WSAETIMEDOUT;
    default: return WSAECONNREFUSED;
    }
}

HsResult socks5(SOCKET s, const sockaddr_in &target, DWORD deadline)
{
    // 1. Приветствие: без авторизации и, если задан логин, логин/пароль.
    const bool withAuth = g_conf.auth && g_conf.user[0] != '\0';
    const std::uint8_t hello[] = {0x05, static_cast<std::uint8_t>(withAuth ? 2 : 1), 0x00, 0x02};
    if (int err = sendAll(s, hello, withAuth ? 4 : 3, deadline))
        return {err, L"приветствие SOCKS5"};

    std::uint8_t method[2];
    if (int err = recvExact(s, method, 2, deadline))
        return {err, L"ответ на приветствие SOCKS5"};
    if (method[0] != 0x05)
        return {WSAECONNREFUSED, L"сервер не SOCKS5"};

    if (method[1] == 0x02) {
        if (!withAuth)
            return {WSAEACCES, L"прокси требует логин и пароль"};
        // 2. RFC 1929: длины влезают в байт — поля не длиннее kAuthLen - 1.
        const auto ulen = static_cast<std::uint8_t>(std::strlen(g_conf.user));
        const auto plen = static_cast<std::uint8_t>(std::strlen(g_conf.pass));
        std::uint8_t auth[3 + 2 * px::kAuthLen];
        int n = 0;
        auth[n++] = 0x01;
        auth[n++] = ulen;
        std::memcpy(auth + n, g_conf.user, ulen);
        n += ulen;
        auth[n++] = plen;
        std::memcpy(auth + n, g_conf.pass, plen);
        n += plen;
        const int err = sendAll(s, auth, n, deadline);
        SecureZeroMemory(auth, sizeof(auth));
        if (err)
            return {err, L"отправка логина SOCKS5"};
        std::uint8_t authRep[2];
        if (int e = recvExact(s, authRep, 2, deadline))
            return {e, L"ответ на логин SOCKS5"};
        if (authRep[0] != 0x01 || authRep[1] != 0x00)
            return {WSAEACCES, L"неверный логин или пароль SOCKS5"};
    } else if (method[1] != 0x00) {
        return {WSAEACCES, L"прокси не принял ни один способ входа SOCKS5"};
    }

    // 3. CONNECT к IPv4-адресу игрового сервера.
    std::uint8_t req[10] = {0x05, 0x01, 0x00, 0x01};
    std::memcpy(req + 4, &target.sin_addr.s_addr, 4);
    std::memcpy(req + 8, &target.sin_port, 2); // уже в сетевом порядке
    if (int err = sendAll(s, req, sizeof(req), deadline))
        return {err, L"запрос CONNECT SOCKS5"};

    std::uint8_t rep[4];
    if (int err = recvExact(s, rep, 4, deadline))
        return {err, L"ответ на CONNECT SOCKS5"};
    if (rep[0] != 0x05)
        return {WSAECONNREFUSED, L"неверный ответ SOCKS5"};
    if (rep[1] != 0x00)
        return {socksReplyToWsa(rep[1]), L"прокси отказал в соединении с сервером игры"};

    // BND.ADDR + BND.PORT — дочитать, чтобы они не попали клиенту.
    int rest = 0;
    if (rep[3] == 0x01) {
        rest = 4 + 2;
    } else if (rep[3] == 0x04) {
        rest = 16 + 2;
    } else if (rep[3] == 0x03) {
        std::uint8_t dlen = 0;
        if (int err = recvExact(s, &dlen, 1, deadline))
            return {err, L"ответ на CONNECT SOCKS5"};
        rest = dlen + 2;
    } else {
        return {WSAECONNREFUSED, L"неверный тип адреса в ответе SOCKS5"};
    }
    std::uint8_t bnd[255 + 2];
    if (int err = recvExact(s, bnd, rest, deadline))
        return {err, L"ответ на CONNECT SOCKS5"};
    return {};
}

void base64(const char *in, size_t len, char *out, size_t outSize)
{
    static const char kAbc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < len && o + 4 < outSize; i += 3) {
        const std::uint32_t b0 = static_cast<std::uint8_t>(in[i]);
        const std::uint32_t b1 = i + 1 < len ? static_cast<std::uint8_t>(in[i + 1]) : 0;
        const std::uint32_t b2 = i + 2 < len ? static_cast<std::uint8_t>(in[i + 2]) : 0;
        const std::uint32_t v = (b0 << 16) | (b1 << 8) | b2;
        out[o++] = kAbc[(v >> 18) & 0x3F];
        out[o++] = kAbc[(v >> 12) & 0x3F];
        out[o++] = i + 1 < len ? kAbc[(v >> 6) & 0x3F] : '=';
        out[o++] = i + 2 < len ? kAbc[v & 0x3F] : '=';
    }
    out[o] = '\0';
}

HsResult httpConnect(SOCKET s, const sockaddr_in &target, DWORD deadline)
{
    char ip[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &target.sin_addr, ip, sizeof(ip));
    const unsigned port = ntohs(target.sin_port);

    char auth[512] = {};
    if (g_conf.auth && g_conf.user[0] != '\0') {
        char creds[2 * px::kAuthLen];
        const int n = _snprintf_s(creds, _TRUNCATE, "%s:%s", g_conf.user, g_conf.pass);
        char b64[(sizeof(creds) + 2) / 3 * 4 + 1];
        base64(creds, n > 0 ? static_cast<size_t>(n) : 0, b64, sizeof(b64));
        _snprintf_s(auth, _TRUNCATE, "Proxy-Authorization: Basic %s\r\n", b64);
        SecureZeroMemory(creds, sizeof(creds));
        SecureZeroMemory(b64, sizeof(b64));
    }

    char req[1024];
    const int reqLen = _snprintf_s(req, _TRUNCATE,
                                   "CONNECT %s:%u HTTP/1.1\r\nHost: %s:%u\r\n%s\r\n",
                                   ip, port, ip, port, auth);
    SecureZeroMemory(auth, sizeof(auth));
    if (reqLen <= 0)
        return {WSAEINVAL, L"запрос HTTP CONNECT"};
    const int sendErr = sendAll(s, req, reqLen, deadline);
    SecureZeroMemory(req, sizeof(req));
    if (sendErr)
        return {sendErr, L"запрос HTTP CONNECT"};

    // Заголовки ответа читаются по байту — ровно до пустой строки, чтобы ни
    // один байт после неё не пропал у клиента.
    char head[kMaxHttpHeader + 1];
    int got = 0;
    for (;;) {
        if (got == kMaxHttpHeader)
            return {WSAECONNREFUSED, L"слишком длинный ответ HTTP-прокси"};
        if (int err = recvExact(s, head + got, 1, deadline))
            return {err, L"ответ HTTP-прокси"};
        ++got;
        if (got >= 4 && std::memcmp(head + got - 4, "\r\n\r\n", 4) == 0)
            break;
    }
    head[got] = '\0';

    // Строка статуса: «HTTP/1.x NNN ...».
    if (got < 12 || std::strncmp(head, "HTTP/1.", 7) != 0 || head[8] != ' ')
        return {WSAECONNREFUSED, L"сервер не HTTP-прокси"};
    const int status = std::atoi(head + 9);
    if (status == 200)
        return {};
    if (status == 407)
        return {WSAEACCES, L"HTTP-прокси требует логин и пароль (407)"};
    return {WSAECONNREFUSED, L"HTTP-прокси отказал в соединении"};
}

// ─────────────────────────── маршрутизация connect ──────────────────────────

sockaddr_in g_proxyAddr = {};
bool g_proxyResolved = false;
CRITICAL_SECTION g_resolveCs;

bool resolveProxy(sockaddr_in &out)
{
    EnterCriticalSection(&g_resolveCs);
    if (!g_proxyResolved) {
        addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo *res = nullptr;
        if (getaddrinfo(g_conf.host, nullptr, &hints, &res) == 0 && res) {
            std::memcpy(&g_proxyAddr, res->ai_addr, sizeof(g_proxyAddr));
            g_proxyAddr.sin_port = htons(g_conf.port);
            g_proxyResolved = true;
        }
        if (res)
            freeaddrinfo(res);
    }
    out = g_proxyAddr;
    const bool ok = g_proxyResolved;
    LeaveCriticalSection(&g_resolveCs);
    return ok;
}

bool isStreamSocket(SOCKET s)
{
    int type = 0;
    int len = sizeof(type);
    return getsockopt(s, SOL_SOCKET, SO_TYPE, reinterpret_cast<char *>(&type), &len) == 0
        && type == SOCK_STREAM;
}

enum class Route { Direct, Proxy, Block };

Route classify(SOCKET s, const sockaddr *name, int namelen)
{
    if (!name || namelen < static_cast<int>(sizeof(sockaddr)))
        return Route::Direct; // ws2_32 сам вернёт ошибку
    if (name->sa_family == AF_INET && namelen >= static_cast<int>(sizeof(sockaddr_in))) {
        const auto *sin = reinterpret_cast<const sockaddr_in *>(name);
        const std::uint32_t ip = ntohl(sin->sin_addr.s_addr);
        if ((ip >> 24) == 127 || ip == INADDR_ANY)
            return Route::Direct; // локальные службы — напрямую
        return isStreamSocket(s) ? Route::Proxy : Route::Direct;
    }
    if (name->sa_family == AF_INET6 && namelen >= static_cast<int>(sizeof(sockaddr_in6))) {
        const auto *sin6 = reinterpret_cast<const sockaddr_in6 *>(name);
        if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))
            return Route::Direct;
        // IPv4 в IPv6 (::ffff:a.b.c.d) тоже пошёл бы мимо прокси.
        return isStreamSocket(s) ? Route::Block : Route::Direct;
    }
    return Route::Direct;
}

int tunnel(SOCKET s, const sockaddr_in &target)
{
    // Регистрация событий клиента — снять на время рукопожатия, иначе
    // клиент получит FD_CONNECT/FD_READ на байты прокси.
    WSAEVENT event = nullptr;
    long mask = 0;
    EnterCriticalSection(&g_sockCs);
    if (SockInfo *e = findSock(s, false)) {
        event = e->event;
        mask = e->mask;
        e->tunneled = false;
        e->fdConnectDue = false;
        e->connectInFlight = false;
    }
    LeaveCriticalSection(&g_sockCs);
    if (event)
        g_eventSelect(s, nullptr, 0); // сокет остаётся неблокирующим

    char targetIp[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &target.sin_addr, targetIp, sizeof(targetIp));
    const unsigned targetPort = ntohs(target.sin_port);
    g_shared->targetAddr = target.sin_addr.s_addr;
    g_shared->targetPort = target.sin_port;

    const DWORD deadline = GetTickCount() + kHandshakeTimeoutMs;
    bool wouldBlock = false;
    HsResult hs;
    sockaddr_in proxy;
    if (!resolveProxy(proxy)) {
        hs = {WSAEHOSTUNREACH, L"не удалось разрешить адрес прокси"};
    } else if (g_connect(s, reinterpret_cast<const sockaddr *>(&proxy), sizeof(proxy)) == SOCKET_ERROR) {
        const int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            wouldBlock = true;
            if (const int e = waitReady(s, true, deadline))
                hs = {e, L"подключение к прокси"};
        } else {
            hs = {err, L"подключение к прокси"};
        }
    }
    if (hs.err == 0) {
        hs = g_conf.type == px::ProxyType::HttpConnect ? httpConnect(s, target, deadline)
                                                       : socks5(s, target, deadline);
    }

    if (hs.err == 0) {
        InterlockedIncrement(reinterpret_cast<volatile LONG *>(&g_shared->connectionsCount));
        setStatus(px::TunnelActive, 0, L"Туннель: %hs:%u → %hs:%u",
                  g_conf.host, g_conf.port, targetIp, targetPort);
    } else {
        setStatus(px::TunnelError, hs.err, L"Прокси %hs:%u → %hs:%u: %ls (код %d)",
                  g_conf.host, g_conf.port, targetIp, targetPort, hs.what, hs.err);
    }

    EnterCriticalSection(&g_sockCs);
    SockInfo *e = findSock(s, true);
    if (e && hs.err == 0) {
        e->tunneled = true;
        e->peer = target;
    }
    const bool viaEvent = event && (mask & FD_CONNECT);
    if (e && viaEvent) {
        e->fdConnectDue = true;
        e->fdConnectError = hs.err;
    } else if (e && wouldBlock && hs.err == 0) {
        e->connectInFlight = true; // FD_CONNECT — если клиент зарегистрирует события позже
    }
    LeaveCriticalSection(&g_sockCs);

    if (event) {
        g_eventSelect(s, event, mask);
        if (viaEvent) {
            // Клиент ждёт FD_CONNECT: итог — в WSAEnumNetworkEvents.
            SetEvent(event);
            WSASetLastError(WSAEWOULDBLOCK);
            return SOCKET_ERROR;
        }
    }
    if (hs.err) {
        WSASetLastError(hs.err);
        return SOCKET_ERROR;
    }
    if (wouldBlock) {
        // Неблокирующий сокет: select() на запись сразу покажет готовность.
        WSASetLastError(WSAEWOULDBLOCK);
        return SOCKET_ERROR;
    }
    return 0;
}

// ─────────────────────────── обработчики ────────────────────────────────────

int WSAAPI hkConnect(SOCKET s, const sockaddr *name, int namelen)
{
    switch (classify(s, name, namelen)) {
    case Route::Proxy:
        return tunnel(s, *reinterpret_cast<const sockaddr_in *>(name));
    case Route::Block:
        WSASetLastError(WSAENETUNREACH);
        return SOCKET_ERROR;
    case Route::Direct:
        break;
    }
    return g_connect(s, name, namelen);
}

int WSAAPI hkWSAConnect(SOCKET s, const sockaddr *name, int namelen, LPWSABUF callerData,
                        LPWSABUF calleeData, LPQOS sqos, LPQOS gqos)
{
    switch (classify(s, name, namelen)) {
    case Route::Proxy:
        // Данные соединения и QoS через прокси не передать (TCP их и не поддерживает).
        if (calleeData)
            calleeData->len = 0;
        return tunnel(s, *reinterpret_cast<const sockaddr_in *>(name));
    case Route::Block:
        WSASetLastError(WSAENETUNREACH);
        return SOCKET_ERROR;
    case Route::Direct:
        break;
    }
    return g_wsaConnect(s, name, namelen, callerData, calleeData, sqos, gqos);
}

int WSAAPI hkGetPeerName(SOCKET s, sockaddr *name, int *namelen)
{
    if (name && namelen && *namelen >= static_cast<int>(sizeof(sockaddr_in))) {
        EnterCriticalSection(&g_sockCs);
        const SockInfo *e = findSock(s, false);
        const bool tunneled = e && e->tunneled;
        const sockaddr_in peer = tunneled ? e->peer : sockaddr_in{};
        LeaveCriticalSection(&g_sockCs);
        if (tunneled) {
            std::memcpy(name, &peer, sizeof(peer));
            *namelen = sizeof(peer);
            return 0;
        }
    }
    return g_getPeerName(s, name, namelen);
}

int WSAAPI hkCloseSocket(SOCKET s)
{
    EnterCriticalSection(&g_sockCs);
    if (SockInfo *e = findSock(s, false))
        *e = SockInfo{};
    LeaveCriticalSection(&g_sockCs);
    return g_closeSocket(s);
}

int WSAAPI hkWSAEventSelect(SOCKET s, WSAEVENT event, long mask)
{
    const int rc = g_eventSelect(s, event, mask);
    if (rc != 0)
        return rc;
    bool signal = false;
    EnterCriticalSection(&g_sockCs);
    // Без свободной записи регистрация не запомнится, и туннель на этом
    // сокете не сможет её восстановить — тогда tunnel() вернёт обычный итог.
    if (SockInfo *e = findSock(s, event != nullptr && mask != 0)) {
        e->event = mask ? event : nullptr;
        e->mask = mask;
        if (e->connectInFlight && e->event && (mask & FD_CONNECT)) {
            e->connectInFlight = false;
            e->fdConnectDue = true;
            e->fdConnectError = 0;
            signal = true;
        }
    }
    LeaveCriticalSection(&g_sockCs);
    if (signal)
        SetEvent(event);
    return rc;
}

int WSAAPI hkWSAEnumNetworkEvents(SOCKET s, WSAEVENT event, LPWSANETWORKEVENTS events)
{
    const int rc = g_enumEvents(s, event, events);
    if (rc != 0 || !events)
        return rc;
    EnterCriticalSection(&g_sockCs);
    if (SockInfo *e = findSock(s, false); e && e->fdConnectDue) {
        e->fdConnectDue = false;
        events->lNetworkEvents |= FD_CONNECT;
        events->iErrorCode[FD_CONNECT_BIT] = e->fdConnectError;
    }
    LeaveCriticalSection(&g_sockCs);
    return rc;
}

// ─────────────────────────── установка ──────────────────────────────────────

struct HookDef {
    const char *name;
    void *detour;
    void **original;
};

bool installHooks()
{
    HMODULE ws2 = GetModuleHandleW(L"ws2_32.dll"); // клиент импортирует её статически
    if (!ws2)
        return false;
    const HookDef hooks[] = {
        {"connect", reinterpret_cast<void *>(hkConnect), reinterpret_cast<void **>(&g_connect)},
        {"WSAConnect", reinterpret_cast<void *>(hkWSAConnect), reinterpret_cast<void **>(&g_wsaConnect)},
        {"getpeername", reinterpret_cast<void *>(hkGetPeerName), reinterpret_cast<void **>(&g_getPeerName)},
        {"closesocket", reinterpret_cast<void *>(hkCloseSocket), reinterpret_cast<void **>(&g_closeSocket)},
        {"WSAEventSelect", reinterpret_cast<void *>(hkWSAEventSelect), reinterpret_cast<void **>(&g_eventSelect)},
        {"WSAEnumNetworkEvents", reinterpret_cast<void *>(hkWSAEnumNetworkEvents),
         reinterpret_cast<void **>(&g_enumEvents)},
    };
    // Сначала — адреса оригиналов, чтобы обработчик, сработавший сразу после
    // установки своего перехвата, не наткнулся на пустой указатель соседнего.
    void *targets[ARRAYSIZE(hooks)];
    for (size_t i = 0; i < ARRAYSIZE(hooks); ++i) {
        targets[i] = reinterpret_cast<void *>(GetProcAddress(ws2, hooks[i].name));
        if (!targets[i])
            return false;
        *hooks[i].original = static_cast<std::uint8_t *>(targets[i]) + 2;
    }
    for (size_t i = 0; i < ARRAYSIZE(hooks); ++i) {
        if (!hotpatch(targets[i], hooks[i].detour))
            return false; // уже стоящие перехваты рабочие; лаунчер закроет клиент
    }
    return true;
}

bool isLoopback4(DWORD addr)
{
    return (ntohl(addr) >> 24) == 127 || addr == 0;
}

// Есть ли у процесса TCP-соединения к нелокальным адресам — открытые до
// установки перехватов, то есть мимо прокси.
bool hasDirectConnections()
{
    const DWORD pid = GetCurrentProcessId();
    const ULONG families[] = {AF_INET, AF_INET6};
    for (ULONG family : families) {
        DWORD size = 0;
        GetExtendedTcpTable(nullptr, &size, FALSE, family, TCP_TABLE_OWNER_PID_ALL, 0);
        if (size == 0)
            continue;
        size += 4096; // таблица может вырасти между вызовами
        auto *buf = static_cast<std::uint8_t *>(HeapAlloc(GetProcessHeap(), 0, size));
        if (!buf)
            return true; // проверить нечем — считаем, что утечка есть
        bool leaked = false;
        if (GetExtendedTcpTable(buf, &size, FALSE, family, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) {
            leaked = true;
        } else if (family == AF_INET) {
            const auto *t = reinterpret_cast<MIB_TCPTABLE_OWNER_PID *>(buf);
            for (DWORD i = 0; i < t->dwNumEntries && !leaked; ++i) {
                const auto &r = t->table[i];
                leaked = r.dwOwningPid == pid && r.dwState != MIB_TCP_STATE_LISTEN
                      && !isLoopback4(r.dwRemoteAddr);
            }
        } else {
            const auto *t = reinterpret_cast<MIB_TCP6TABLE_OWNER_PID *>(buf);
            static const std::uint8_t kLoop6[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
            static const std::uint8_t kAny6[16] = {};
            for (DWORD i = 0; i < t->dwNumEntries && !leaked; ++i) {
                const auto &r = t->table[i];
                leaked = r.dwOwningPid == pid && r.dwState != MIB_TCP_STATE_LISTEN
                      && std::memcmp(r.ucRemoteAddr, kLoop6, 16) != 0
                      && std::memcmp(r.ucRemoteAddr, kAny6, 16) != 0;
            }
        }
        HeapFree(GetProcessHeap(), 0, buf);
        if (leaked)
            return true;
    }
    return false;
}

// Проверка соединений — вне DllMain (iphlpapi под блокировкой загрузчика
// звать нельзя). Перехваты к этому моменту уже стоят, новых утечек нет.
DWORD WINAPI verifyThread(void *)
{
    if (hasDirectConnections())
        setHookStatus(px::HookLeaked, L"Клиент соединился напрямую до подключения прокси");
    else
        setHookStatus(px::HookOk, L"Перехват сети установлен");
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE; // при выходе процесса ОС всё освободит сама
    DisableThreadLibraryCalls(module);
    InitializeCriticalSection(&g_statusCs);
    InitializeCriticalSection(&g_sockCs);
    InitializeCriticalSection(&g_resolveCs);
    for (auto &e : g_socks)
        e.s = INVALID_SOCKET;

    if (!openSharedConfig() || g_conf.type == px::ProxyType::Direct)
        return TRUE; // без настроек ничего не перехватываем

    // Перехваты ссылаются на код библиотеки — выгружать её нельзя никогда.
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                       reinterpret_cast<LPCWSTR>(&DllMain), &pinned);

    if (!installHooks()) {
        setHookStatus(px::HookFailed, L"Не удалось перехватить функции ws2_32.dll");
        return TRUE;
    }
    if (HANDLE t = CreateThread(nullptr, 0, verifyThread, nullptr, 0, nullptr))
        CloseHandle(t);
    else
        setHookStatus(px::HookFailed, L"Не удалось запустить проверку соединений");
    return TRUE;
}
