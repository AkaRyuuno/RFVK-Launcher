#pragma once
// Общая память лаунчера (x64) и библиотеки перехвата сети rfvk_proxy.dll (x86).
//
// Лаунчер создаёт именованное отображение «Local\RFVK.Proxy.<pid>» до старта
// или сразу при создании процесса клиента и помещает туда настройки прокси.
// Библиотека открывает отображение при загрузке, перехватывает connect/WSAConnect
// в ws2_32.dll и прозрачно направляет TCP-соединения через SOCKS5 или HTTP-туннель.
//
// Обе стороны используют фиксированные размеры типов без указателей.

#include <cstdint>

namespace rfvk_proxy {

constexpr std::uint32_t kMagic = 0x58504652; // 'RFPX' (RF Proxy)
constexpr std::uint32_t kVersion = 1;

// Имя отображения: kMappingPrefix + десятичный PID клиента.
constexpr wchar_t kMappingPrefix[] = L"Local\\RFVK.Proxy.";

enum class ProxyType : std::uint32_t {
    Direct = 0,      // Прямое подключение без прокси (нативный IP)
    Socks5 = 1,      // SOCKS5 (RFC 1928, RFC 1929 с авторизацией)
    HttpConnect = 2  // HTTP CONNECT туннелирование
};

enum ProxyState : std::int32_t {
    StateInit = 0,     // Ожидание первого подключения
    StateActive = 1,   // Прокси успешно работает, туннель активен
    StateError = 2     // Ошибка подключения к прокси или сбой авторизации
};

constexpr int kHostLen = 256;
constexpr int kAuthLen = 128;
constexpr int kStatusLen = 256;

#pragma pack(push, 4)
struct SharedConfig {
    std::uint32_t magic;      // kMagic
    std::uint32_t version;    // kVersion
    std::uint32_t size;       // sizeof(SharedConfig)

    // ── лаунчер → DLL ──
    std::uint32_t type;            // ProxyType
    char proxyHost[kHostLen];      // IP-адрес или домен прокси (ASCII/UTF-8)
    std::uint16_t proxyPort;       // Порт прокси (host byte order, например 1080)
    std::uint8_t authEnabled;      // 1 — логин/пароль включены, 0 — без авторизации
    std::uint8_t reserved[5];      // Выравнивание

    char username[kAuthLen];       // Логин прокси
    char password[kAuthLen];       // Пароль прокси

    // ── DLL → лаунчер (диагностика и статистика) ──
    volatile std::int32_t state;            // ProxyState
    volatile std::int32_t connectionsCount; // Количество успешно перехваченных соединений
    volatile std::int32_t lastError;        // Код последней ошибки Winsock / handshake
    char targetHost[kHostLen];              // Последний целевой хост / IP игры
    std::uint16_t targetPort;               // Последний целевой порт игры
    std::uint16_t reserved2;
    wchar_t statusText[kStatusLen];         // Описание текущего статуса (UTF-16)
};
#pragma pack(pop)

static_assert(sizeof(SharedConfig) % 4 == 0, "SharedConfig должен быть выровнен по 4 байта");

} // namespace rfvk_proxy
