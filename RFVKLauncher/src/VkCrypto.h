#pragma once
#include <QByteArray>

// Нативная криптография Windows (без внешних зависимостей):
//  • DPAPI (crypt32) — хранение refresh-токенов под текущим пользователем;
//  • AES-256-GCM (CNG/bcrypt) — расшифровка cookie Chromium (v10).
namespace VkCrypto {

// DPAPI: шифрование/расшифровка привязаны к текущему пользователю Windows.
QByteArray dpapiProtect(const QByteArray &plain);
QByteArray dpapiUnprotect(const QByteArray &blob);

// AES-256-GCM. key=32 байта, nonce=12 байт, tag=16 байт. Пусто при ошибке.
QByteArray aesGcmDecrypt(const QByteArray &key, const QByteArray &nonce,
                         const QByteArray &cipher, const QByteArray &tag);

} // namespace VkCrypto
