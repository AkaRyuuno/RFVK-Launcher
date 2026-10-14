#include "VkCrypto.h"

#include <windows.h>
#include <wincrypt.h> // DPAPI: CryptProtectData / CryptUnprotectData
#include <bcrypt.h>   // AES-GCM

namespace VkCrypto {

QByteArray dpapiProtect(const QByteArray &plain)
{
    DATA_BLOB in{static_cast<DWORD>(plain.size()),
                 reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()))};
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"rfvk", nullptr, nullptr, nullptr, 0, &out))
        return {};
    QByteArray r(reinterpret_cast<char *>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return r;
}

QByteArray dpapiUnprotect(const QByteArray &blob)
{
    DATA_BLOB in{static_cast<DWORD>(blob.size()),
                 reinterpret_cast<BYTE *>(const_cast<char *>(blob.constData()))};
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out))
        return {};
    QByteArray r(reinterpret_cast<char *>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return r;
}

QByteArray aesGcmDecrypt(const QByteArray &key, const QByteArray &nonce,
                         const QByteArray &cipher, const QByteArray &tag)
{
    if (key.size() != 32 || nonce.size() != 12 || tag.size() != 16)
        return {};

    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0)
        return {};
    struct AlgGuard {
        BCRYPT_ALG_HANDLE a;
        ~AlgGuard() { if (a) BCryptCloseAlgorithmProvider(a, 0); }
    } algGuard{alg};

    if (BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
                          reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_GCM)),
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0)
        != 0)
        return {};

    BCRYPT_KEY_HANDLE keyh = nullptr;
    if (BCryptGenerateSymmetricKey(alg, &keyh, nullptr, 0,
                                   reinterpret_cast<PUCHAR>(const_cast<char *>(key.constData())),
                                   static_cast<ULONG>(key.size()), 0)
        != 0)
        return {};
    struct KeyGuard {
        BCRYPT_KEY_HANDLE k;
        ~KeyGuard() { if (k) BCryptDestroyKey(k); }
    } keyGuard{keyh};

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = reinterpret_cast<PUCHAR>(const_cast<char *>(nonce.constData()));
    info.cbNonce = static_cast<ULONG>(nonce.size());
    info.pbTag = reinterpret_cast<PUCHAR>(const_cast<char *>(tag.constData()));
    info.cbTag = static_cast<ULONG>(tag.size());

    QByteArray out(cipher.size(), Qt::Uninitialized);
    ULONG written = 0;
    const NTSTATUS st =
        BCryptDecrypt(keyh, reinterpret_cast<PUCHAR>(const_cast<char *>(cipher.constData())),
                      static_cast<ULONG>(cipher.size()), &info, nullptr, 0,
                      reinterpret_cast<PUCHAR>(out.data()), static_cast<ULONG>(out.size()),
                      &written, 0);
    if (st != 0)
        return {};
    out.resize(static_cast<int>(written));
    return out;
}

} // namespace VkCrypto
