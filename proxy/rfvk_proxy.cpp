// rfvk_proxy.dll — высокопроизводительная 32-битная библиотека перехвата сетевого трафика.
//
// Инжектируется лаунчером RFVK в 32-битный клиент RF_Online.exe.
// Перехватывает вызовы connect, WSAConnect и getpeername в ws2_32.dll:
// - Локальные соединения (127.0.0.1) пропускаются напрямую без задержек.
// - Если прокси выключен (Direct) — нулевой оверхед, прямое подключение.
// - При активном SOCKS5 (RFC 1928, RFC 1929) или HTTP CONNECT:
//   прозрачно выполняет рукопожатие с прокси-сервером и связывает сокет
//   с удалённым сервером игры. После рукопожатия поток данных идёт на
//   полной нативной скорости без модификаций пакетов.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>

#include "rfvk_proxy_shared.h"

#pragma comment(lib, "ws2_32.lib")

namespace {

template <typename T, size_t N>
constexpr size_t countOf(T const (&)[N]) noexcept {
    return N;
}

// ── Общая конфигурация из лаунчера ──────────────────────────────────────────
HANDLE g_mapHandle = nullptr;
rfvk_proxy::SharedConfig *g_cfg = nullptr;

// ── Структура перехвата (Hotpatch 5-byte JMP Trampoline) ─────────────────────
#pragma pack(push, 1)
struct Trampoline {
    uint8_t prologue[5]; // Скопированные оригинальные инструкции (8B FF 55 8B EC)
    uint8_t jmpOpcode;   // 0xE9
    uint32_t jmpRel;     // Относительное смещение к original_func + 5
};
#pragma pack(pop)

using FnConnect = int(WSAAPI *)(SOCKET s, const struct sockaddr *name, int namelen);
using FnWSAConnect = int(WSAAPI *)(SOCKET s, const struct sockaddr *name, int namelen,
                                   LPWSABUF lpCallerData, LPWSABUF lpCalleeData,
                                   LPQOS lpSQOS, LPQOS lpGQOS);
using FnGetPeerName = int(WSAAPI *)(SOCKET s, struct sockaddr *name, int *namelen);
using FnCloseSocket = int(WSAAPI *)(SOCKET s);

static FnConnect g_origConnect = nullptr;
static FnWSAConnect g_origWSAConnect = nullptr;
static FnGetPeerName g_origGetPeerName = nullptr;
static FnCloseSocket g_origCloseSocket = nullptr;

static Trampoline *g_trampConnect = nullptr;
static Trampoline *g_trampWSAConnect = nullptr;
static Trampoline *g_trampGetPeerName = nullptr;
static Trampoline *g_trampCloseSocket = nullptr;

// ── Отслеживание сокетов для прозрачного getpeername ─────────────────────────
struct SocketPeerEntry {
    SOCKET s;
    sockaddr_in targetAddr;
};

constexpr int kMaxTrackedSockets = 64;
static SocketPeerEntry g_peers[kMaxTrackedSockets];
static CRITICAL_SECTION g_cs;

void setTrackedPeer(SOCKET s, const sockaddr_in &addr) {
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < kMaxTrackedSockets; ++i) {
        if (g_peers[i].s == 0 || g_peers[i].s == INVALID_SOCKET || g_peers[i].s == s) {
            g_peers[i].s = s;
            g_peers[i].targetAddr = addr;
            break;
        }
    }
    LeaveCriticalSection(&g_cs);
}

bool getTrackedPeer(SOCKET s, sockaddr_in &outAddr) {
    bool found = false;
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < kMaxTrackedSockets; ++i) {
        if (g_peers[i].s == s) {
            outAddr = g_peers[i].targetAddr;
            found = true;
            break;
        }
    }
    LeaveCriticalSection(&g_cs);
    return found;
}

void removeTrackedPeer(SOCKET s) {
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < kMaxTrackedSockets; ++i) {
        if (g_peers[i].s == s) {
            g_peers[i].s = INVALID_SOCKET;
            break;
        }
    }
    LeaveCriticalSection(&g_cs);
}

// ── Установка 5-байтного хотпатч-хука ─────────────────────────────────────────
Trampoline *installHook(void *targetFunc, void *hookFunc) {
    if (!targetFunc || !hookFunc)
        return nullptr;

    auto *target = reinterpret_cast<uint8_t *>(targetFunc);

    // Проверяем классический пролог Windows API: 8B FF (mov edi, edi), 55 (push ebp), 8B EC (mov ebp, esp)
    if (target[0] != 0x8B || target[1] != 0xFF || target[2] != 0x55 || target[3] != 0x8B || target[4] != 0xEC) {
        // Если функция не совпадает с ожидаемым прологом, всё равно можно поставить JMP,
        // если первые 5 байт не содержат RIP/rel инструкций.
    }

    auto *tramp = reinterpret_cast<Trampoline *>(
        VirtualAlloc(nullptr, sizeof(Trampoline), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!tramp)
        return nullptr;

    // Копируем первые 5 байт оригинальной функции
    memcpy(tramp->prologue, target, 5);
    tramp->jmpOpcode = 0xE9; // JMP rel32
    tramp->jmpRel = static_cast<uint32_t>((target + 5) - (reinterpret_cast<uint8_t *>(&tramp->jmpOpcode) + 5));

    // Устанавливаем перехват в целевой функции
    DWORD oldProtect = 0;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        VirtualFree(tramp, 0, MEM_RELEASE);
        return nullptr;
    }

    target[0] = 0xE9; // JMP rel32
    *reinterpret_cast<uint32_t *>(target + 1) =
        static_cast<uint32_t>(reinterpret_cast<uint8_t *>(hookFunc) - (target + 5));

    VirtualProtect(target, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), target, 5);
    FlushInstructionCache(GetCurrentProcess(), tramp, sizeof(Trampoline));

    return tramp;
}

// ── Вспомогательные сетевые функции ──────────────────────────────────────────

int recvExact(SOCKET s, char *buf, int len) {
    int total = 0;
    while (total < len) {
        int r = recv(s, buf + total, len - total, 0);
        if (r <= 0)
            return total;
        total += r;
    }
    return total;
}

// Быстрое кодирование Base64 на стеке без динамических аллокаций памяти
void base64Encode(const char *in, int inLen, char *out, int outMax) {
    static const char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int o = 0;
    for (int i = 0; i < inLen && o + 4 < outMax; i += 3) {
        uint32_t b0 = static_cast<uint8_t>(in[i]);
        uint32_t b1 = (i + 1 < inLen) ? static_cast<uint8_t>(in[i + 1]) : 0;
        uint32_t b2 = (i + 2 < inLen) ? static_cast<uint8_t>(in[i + 2]) : 0;
        uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

        out[o++] = kTable[(triple >> 18) & 0x3F];
        out[o++] = kTable[(triple >> 12) & 0x3F];
        out[o++] = (i + 1 < inLen) ? kTable[(triple >> 6) & 0x3F] : '=';
        out[o++] = (i + 2 < inLen) ? kTable[triple & 0x3F] : '=';
    }
    out[o] = '\0';
}

// Разрешение хоста прокси (поддержка IP-адресов и DNS)
bool resolveProxy(const char *host, uint16_t port, sockaddr_in &outAddr) {
    memset(&outAddr, 0, sizeof(outAddr));
    outAddr.sin_family = AF_INET;
    outAddr.sin_port = htons(port);

    unsigned long ip = inet_addr(host);
    if (ip != INADDR_NONE) {
        outAddr.sin_addr.s_addr = ip;
        return true;
    }

    struct hostent *he = gethostbyname(host);
    if (he && he->h_addr_list && he->h_addr_list[0]) {
        memcpy(&outAddr.sin_addr, he->h_addr_list[0], he->h_length);
        return true;
    }

    return false;
}

// ── Выполнение SOCKS5 рукопожатия (RFC 1928 / RFC 1929) ──────────────────────
bool performSocks5Handshake(SOCKET s, const sockaddr_in &target) {
    // 1. Приветствие клиента (Greeting)
    uint8_t greeting[4];
    int greetLen = 0;
    greeting[0] = 0x05; // SOCKS Version 5

    if (g_cfg->authEnabled && g_cfg->username[0] != '\0') {
        greeting[1] = 0x02; // 2 метода
        greeting[2] = 0x00; // NO AUTH
        greeting[3] = 0x02; // USER/PASSWORD
        greetLen = 4;
    } else {
        greeting[1] = 0x01; // 1 метод
        greeting[2] = 0x00; // NO AUTH
        greetLen = 3;
    }

    if (send(s, reinterpret_cast<const char *>(greeting), greetLen, 0) != greetLen)
        return false;

    // 2. Ответ сервера с выбранным методом
    uint8_t methodResp[2] = {};
    if (recvExact(s, reinterpret_cast<char *>(methodResp), 2) != 2)
        return false;

    if (methodResp[0] != 0x05)
        return false;

    if (methodResp[1] == 0xFF) // No acceptable methods
        return false;

    // 3. Авторизация по логину и паролю при необходимости (RFC 1929)
    if (methodResp[1] == 0x02) {
        char authBuf[512];
        const auto ulen = static_cast<uint8_t>(strlen(g_cfg->username));
        const auto plen = static_cast<uint8_t>(strlen(g_cfg->password));

        authBuf[0] = 0x01; // Версия суб-согласования
        authBuf[1] = static_cast<char>(ulen);
        memcpy(&authBuf[2], g_cfg->username, ulen);
        authBuf[2 + ulen] = static_cast<char>(plen);
        memcpy(&authBuf[3 + ulen], g_cfg->password, plen);

        const int toSend = 3 + ulen + plen;
        if (send(s, authBuf, toSend, 0) != toSend)
            return false;

        uint8_t authResp[2] = {};
        if (recvExact(s, reinterpret_cast<char *>(authResp), 2) != 2)
            return false;

        if (authResp[1] != 0x00) // 0x00 = success
            return false;
    }

    // 4. Запрос подключения (CONNECT request)
    uint8_t req[10];
    req[0] = 0x05; // VER = 5
    req[1] = 0x01; // CMD = CONNECT
    req[2] = 0x00; // RSV = 0
    req[3] = 0x01; // ATYP = IPv4
    memcpy(&req[4], &target.sin_addr.s_addr, 4);
    memcpy(&req[8], &target.sin_port, 2); // уже в сетевом порядке байт

    if (send(s, reinterpret_cast<const char *>(req), 10, 0) != 10)
        return false;

    // 5. Ответ сервера SOCKS5
    uint8_t rep[4] = {};
    if (recvExact(s, reinterpret_cast<char *>(rep), 4) != 4)
        return false;

    if (rep[0] != 0x05 || rep[1] != 0x00) // rep[1] == 0x00 успех
        return false;

    // Считываем адрес привязки (BND.ADDR + BND.PORT), чтобы очистить сокет
    if (rep[3] == 0x01) { // IPv4: 4 байта IP + 2 байта порт
        uint8_t bnd[6];
        recvExact(s, reinterpret_cast<char *>(bnd), 6);
    } else if (rep[3] == 0x03) { // Домен
        uint8_t dlen = 0;
        recvExact(s, reinterpret_cast<char *>(&dlen), 1);
        char dom[260];
        recvExact(s, dom, dlen + 2);
    } else if (rep[3] == 0x04) { // IPv6
        uint8_t bnd6[18];
        recvExact(s, reinterpret_cast<char *>(bnd6), 18);
    }

    return true;
}

// ── Выполнение HTTP CONNECT рукопожатия ───────────────────────────────────────
bool performHttpConnectHandshake(SOCKET s, const sockaddr_in &target) {
    char targetIp[64] = {};
    const char *ntoaRes = inet_ntoa(target.sin_addr);
    if (ntoaRes)
        strncpy_s(targetIp, ntoaRes, _TRUNCATE);
    const uint16_t targetPort = ntohs(target.sin_port);

    char req[1024];
    int reqLen = 0;

    if (g_cfg->authEnabled && g_cfg->username[0] != '\0') {
        char creds[256];
        _snprintf_s(creds, sizeof(creds), _TRUNCATE, "%s:%s", g_cfg->username, g_cfg->password);
        char b64[384];
        base64Encode(creds, static_cast<int>(strlen(creds)), b64, sizeof(b64));

        reqLen = _snprintf_s(req, sizeof(req), _TRUNCATE,
                             "CONNECT %s:%u HTTP/1.1\r\n"
                             "Host: %s:%u\r\n"
                             "Proxy-Authorization: Basic %s\r\n"
                             "Proxy-Connection: Keep-Alive\r\n"
                             "\r\n",
                             targetIp, targetPort, targetIp, targetPort, b64);
    } else {
        reqLen = _snprintf_s(req, sizeof(req), _TRUNCATE,
                             "CONNECT %s:%u HTTP/1.1\r\n"
                             "Host: %s:%u\r\n"
                             "Proxy-Connection: Keep-Alive\r\n"
                             "\r\n",
                             targetIp, targetPort, targetIp, targetPort);
    }

    if (send(s, req, reqLen, 0) != reqLen)
        return false;

    // Считываем заголовки ответа до \r\n\r\n
    char resp[1024] = {};
    int received = 0;
    while (received < sizeof(resp) - 1) {
        char c = 0;
        int r = recv(s, &c, 1, 0);
        if (r <= 0)
            return false;
        resp[received++] = c;
        if (received >= 4 && resp[received - 4] == '\r' && resp[received - 3] == '\n'
            && resp[received - 2] == '\r' && resp[received - 1] == '\n') {
            break;
        }
    }
    resp[received] = '\0';

    // Проверяем статус 200 (например, "HTTP/1.1 200 Connection established")
    if (strstr(resp, " 200 ") != nullptr || strstr(resp, " 200\r\n") != nullptr)
        return true;

    return false;
}

// ── Основная логика маршрутизации сокета ─────────────────────────────────────
int routeSocketConnection(SOCKET s, const struct sockaddr *name, int namelen) {
    if (!name || namelen < sizeof(sockaddr_in))
        return g_origConnect(s, name, namelen);

    const auto *targetSin = reinterpret_cast<const sockaddr_in *>(name);

    // Только IPv4 TCP
    if (targetSin->sin_family != AF_INET)
        return g_origConnect(s, name, namelen);

    // Локальные адреса (127.x.x.x) пропускаются напрямую без модификаций
    const uint32_t ipBytes = ntohl(targetSin->sin_addr.s_addr);
    if ((ipBytes >> 24) == 127 || targetSin->sin_addr.s_addr == INADDR_ANY)
        return g_origConnect(s, name, namelen);

    // Если прокси выключен лаунчером (Direct) — обычное прямое подключение
    if (!g_cfg || g_cfg->magic != rfvk_proxy::kMagic
        || g_cfg->type == static_cast<uint32_t>(rfvk_proxy::ProxyType::Direct)) {
        return g_origConnect(s, name, namelen);
    }

    // Запоминаем целевой хост и порт для лаунчера
    const char *targetStr = inet_ntoa(targetSin->sin_addr);
    if (targetStr)
        strncpy_s(g_cfg->targetHost, targetStr, _TRUNCATE);
    g_cfg->targetPort = ntohs(targetSin->sin_port);

    // Определяем адрес прокси-сервера
    sockaddr_in proxySin = {};
    if (!resolveProxy(g_cfg->proxyHost, g_cfg->proxyPort, proxySin)) {
        g_cfg->state = rfvk_proxy::StateError;
        g_cfg->lastError = WSAEHOSTUNREACH;
        _snwprintf_s(g_cfg->statusText, countOf(g_cfg->statusText), _TRUNCATE,
                     L"Не удалось разрешить адрес прокси %hs", g_cfg->proxyHost);
        WSASetLastError(WSAEHOSTUNREACH);
        return SOCKET_ERROR;
    }

    // Устанавливаем таймаут 5 секунд на время рукопожатия, чтобы не зависать
    DWORD timeoutMs = 5000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs));

    // Подключаемся к прокси-серверу через оригинальный connect
    int rc = g_origConnect(s, reinterpret_cast<const struct sockaddr *>(&proxySin), sizeof(proxySin));
    if (rc != 0) {
        int err = WSAGetLastError();
        g_cfg->state = rfvk_proxy::StateError;
        g_cfg->lastError = err;
        _snwprintf_s(g_cfg->statusText, countOf(g_cfg->statusText), _TRUNCATE,
                     L"Сбой подключения к прокси %hs:%u (код %d)", g_cfg->proxyHost, g_cfg->proxyPort, err);
        return SOCKET_ERROR;
    }

    // Выполняем рукопожатие туннеля
    bool handshakeOk = false;
    if (g_cfg->type == static_cast<uint32_t>(rfvk_proxy::ProxyType::Socks5)) {
        handshakeOk = performSocks5Handshake(s, *targetSin);
    } else if (g_cfg->type == static_cast<uint32_t>(rfvk_proxy::ProxyType::HttpConnect)) {
        handshakeOk = performHttpConnectHandshake(s, *targetSin);
    }

    // Восстанавливаем дефолтный таймаут сокета
    DWORD zero = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&zero), sizeof(zero));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&zero), sizeof(zero));

    if (!handshakeOk) {
        g_cfg->state = rfvk_proxy::StateError;
        g_cfg->lastError = WSAECONNREFUSED;
        _snwprintf_s(g_cfg->statusText, countOf(g_cfg->statusText), _TRUNCATE,
                     L"Ошибка рукопожатия через прокси %hs:%u", g_cfg->proxyHost, g_cfg->proxyPort);
        WSASetLastError(WSAECONNREFUSED);
        return SOCKET_ERROR;
    }

    // Сохраняем исходный целевой адрес сокета для прозрачной работы getpeername
    setTrackedPeer(s, *targetSin);

    // Успех!
    g_cfg->state = rfvk_proxy::StateActive;
    g_cfg->lastError = 0;
    InterlockedIncrement(reinterpret_cast<volatile LONG *>(&g_cfg->connectionsCount));
    _snwprintf_s(g_cfg->statusText, countOf(g_cfg->statusText), _TRUNCATE,
                 L"Туннель активен: %hs:%u → %hs:%u", g_cfg->proxyHost, g_cfg->proxyPort,
                 g_cfg->targetHost, g_cfg->targetPort);

    return 0; // Успешное установление соединения
}

// ── Перехваченные функции Winsock ───────────────────────────────────────────

int WSAAPI Hooked_connect(SOCKET s, const struct sockaddr *name, int namelen) {
    return routeSocketConnection(s, name, namelen);
}

int WSAAPI Hooked_WSAConnect(SOCKET s, const struct sockaddr *name, int namelen,
                             LPWSABUF lpCallerData, LPWSABUF lpCalleeData,
                             LPQOS lpSQOS, LPQOS lpGQOS) {
    return routeSocketConnection(s, name, namelen);
}

int WSAAPI Hooked_getpeername(SOCKET s, struct sockaddr *name, int *namelen) {
    if (name && namelen && *namelen >= sizeof(sockaddr_in)) {
        sockaddr_in tracked;
        if (getTrackedPeer(s, tracked)) {
            memcpy(name, &tracked, sizeof(sockaddr_in));
            *namelen = sizeof(sockaddr_in);
            return 0;
        }
    }
    return g_origGetPeerName(s, name, namelen);
}

int WSAAPI Hooked_closesocket(SOCKET s) {
    removeTrackedPeer(s);
    return g_origCloseSocket(s);
}

// ── Инициализация перехватов ────────────────────────────────────────────────
void initHooks() {
    InitializeCriticalSection(&g_cs);
    for (int i = 0; i < kMaxTrackedSockets; ++i) {
        g_peers[i].s = INVALID_SOCKET;
    }

    HMODULE hWs2 = GetModuleHandleW(L"ws2_32.dll");
    if (!hWs2)
        hWs2 = LoadLibraryW(L"ws2_32.dll");
    if (!hWs2)
        return;

    void *pConnect = GetProcAddress(hWs2, "connect");
    void *pWSAConnect = GetProcAddress(hWs2, "WSAConnect");
    void *pGetPeerName = GetProcAddress(hWs2, "getpeername");
    void *pCloseSocket = GetProcAddress(hWs2, "closesocket");

    if (pConnect) {
        g_trampConnect = installHook(pConnect, reinterpret_cast<void *>(Hooked_connect));
        g_origConnect = reinterpret_cast<FnConnect>(g_trampConnect);
    }
    if (pWSAConnect) {
        g_trampWSAConnect = installHook(pWSAConnect, reinterpret_cast<void *>(Hooked_WSAConnect));
        g_origWSAConnect = reinterpret_cast<FnWSAConnect>(g_trampWSAConnect);
    }
    if (pGetPeerName) {
        g_trampGetPeerName = installHook(pGetPeerName, reinterpret_cast<void *>(Hooked_getpeername));
        g_origGetPeerName = reinterpret_cast<FnGetPeerName>(g_trampGetPeerName);
    }
    if (pCloseSocket) {
        g_trampCloseSocket = installHook(pCloseSocket, reinterpret_cast<void *>(Hooked_closesocket));
        g_origCloseSocket = reinterpret_cast<FnCloseSocket>(g_trampCloseSocket);
    }
}

void openSharedConfig() {
    wchar_t mapName[64];
    _snwprintf_s(mapName, countOf(mapName), _TRUNCATE, L"%ls%lu", rfvk_proxy::kMappingPrefix, GetCurrentProcessId());

    g_mapHandle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mapName);
    if (!g_mapHandle)
        return;

    g_cfg = static_cast<rfvk_proxy::SharedConfig *>(
        MapViewOfFile(g_mapHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(rfvk_proxy::SharedConfig)));

    if (g_cfg) {
        if (g_cfg->magic != rfvk_proxy::kMagic || g_cfg->version != rfvk_proxy::kVersion) {
            UnmapViewOfFile(g_cfg);
            g_cfg = nullptr;
        } else {
            g_cfg->state = rfvk_proxy::StateInit;
            _snwprintf_s(g_cfg->statusText, countOf(g_cfg->statusText), _TRUNCATE,
                         L"rfvk_proxy.dll загружен (PID %lu)", GetCurrentProcessId());
        }
    }
}

} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        openSharedConfig();
        initHooks();
        break;

    case DLL_PROCESS_DETACH:
        if (g_cfg) {
            UnmapViewOfFile(g_cfg);
            g_cfg = nullptr;
        }
        if (g_mapHandle) {
            CloseHandle(g_mapHandle);
            g_mapHandle = nullptr;
        }
        DeleteCriticalSection(&g_cs);
        break;
    }
    return TRUE;
}
