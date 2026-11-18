#pragma once
// Общая память лаунчера (x64) и библиотеки перехвата сети rfvk_proxy.dll (x86).
//
// Лаунчер создаёт именованное отображение «Local\RFVK.Proxy.<pid>» сразу после
// старта клиента, заполняет настройки прокси и подключает библиотеку.
// Библиотека один раз копирует настройки при загрузке, ставит перехваты в
// ws2_32.dll и сообщает итог в hookStatus. Пока hookStatus не стал HookOk,
// лаунчер считает клиент неподтверждённым; HookFailed/HookLeaked или
// таймаут — лаунчер закрывает клиент, чтобы тот не играл с реального IP.
//
// Обе стороны используют фиксированные размеры типов без указателей.

#include <cstdint>

namespace rfvk_proxy {

constexpr std::uint32_t kMagic = 0x58504652; // 'RFPX' (RF Proxy)
constexpr std::uint32_t kVersion = 3;

// Имя отображения: kMappingPrefix + десятичный PID клиента.
constexpr wchar_t kMappingPrefix[] = L"Local\\RFVK.Proxy.";

enum class ProxyType : std::uint32_t {
    Direct = 0,      // без прокси — библиотека не подключается
    Socks5 = 1,      // SOCKS5 (RFC 1928, логин/пароль — RFC 1929)
    HttpConnect = 2  // HTTP CONNECT
};

// Итог установки перехватов (пишет библиотека один раз).
enum HookStatus : std::int32_t {
    HookPending = 0, // библиотека ещё не загрузилась или проверяет соединения
    HookOk = 1,      // перехваты стоят, прямых соединений до них не было
    HookFailed = 2,  // перехваты не встали — трафик пошёл бы мимо прокси
    HookLeaked = 3   // до перехватов клиент уже соединился напрямую
};

// Состояние последнего туннеля (диагностика для журнала).
enum TunnelState : std::int32_t {
    TunnelIdle = 0,   // ещё не было соединений
    TunnelActive = 1, // последнее соединение прошло через прокси
    TunnelError = 2   // последнее соединение через прокси не удалось
};

constexpr int kHostLen = 256;
constexpr int kAuthLen = 128;
constexpr int kStatusLen = 256;

#pragma pack(push, 4)
struct SharedConfig {
    std::uint32_t magic;   // kMagic
    std::uint32_t version; // kVersion
    std::uint32_t size;    // sizeof(SharedConfig)

    // ── лаунчер → библиотека (читается один раз при загрузке) ──
    std::uint32_t type;       // ProxyType
    char proxyHost[kHostLen]; // IP или домен прокси (UTF-8, с завершающим нулём)
    std::uint16_t proxyPort;  // порт прокси (host byte order)
    std::uint8_t authEnabled; // 1 — логин/пароль
    std::uint8_t reserved[5];
    char username[kAuthLen];  // логин (UTF-8)
    char password[kAuthLen];  // пароль (UTF-8)

    // ── библиотека → лаунчер ──
    volatile std::int32_t hookStatus;       // HookStatus
    volatile std::int32_t tunnelState;      // TunnelState
    volatile std::int32_t connectionsCount; // успешные туннели
    volatile std::int32_t lastError;        // код Winsock последней ошибки
    // Последний сервер игры, к которому клиент шёл через прокси (сетевой
    // порядок байт): по нему лаунчер проверяет прокси кнопкой «Проверить».
    volatile std::uint32_t targetAddr;
    volatile std::uint16_t targetPort;
    std::uint16_t reserved2;
    wchar_t statusText[kStatusLen];         // последнее событие (UTF-16)
};
#pragma pack(pop)

static_assert(sizeof(SharedConfig) % 4 == 0, "SharedConfig должен быть выровнен по 4 байта");

} // namespace rfvk_proxy
