#include "crypto.h"
#include <bcrypt.h>
#include <wincrypt.h>
#include <cstring>
#include <memory>

namespace crypto
{
namespace
{
constexpr ULONG NoFlags = 0;

struct LocalDeleter
{
    void operator()(void* pointer) const
    {
        LocalFree(pointer);
    }
};

struct KeyDeleter
{
    void operator()(BCRYPT_KEY_HANDLE key) const
    {
        BCryptDestroyKey(key);
    }
};

using PublicKeyInfo = std::unique_ptr<CERT_PUBLIC_KEY_INFO, LocalDeleter>;
using KeyHandle = std::unique_ptr<void, KeyDeleter>;

PublicKeyInfo DecodeSpki(std::span<const uint8_t> spkiDer)
{
    CERT_PUBLIC_KEY_INFO* info = nullptr;
    DWORD size = 0;
    if (!CryptDecodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, spkiDer.data(),
            static_cast<DWORD>(spkiDer.size()), CRYPT_DECODE_ALLOC_FLAG, nullptr, &info, &size))
    {
        return nullptr;
    }
    return PublicKeyInfo(info);
}

bool IsP256(const CERT_PUBLIC_KEY_INFO& info)
{
    const CRYPT_OBJID_BLOB& parameters = info.Algorithm.Parameters;
    LPSTR curve = nullptr;
    DWORD size = 0;
    if (std::strcmp(info.Algorithm.pszObjId, szOID_ECC_PUBLIC_KEY) != 0 ||
        !CryptDecodeObjectEx(X509_ASN_ENCODING, X509_OBJECT_IDENTIFIER, parameters.pbData, parameters.cbData,
            CRYPT_DECODE_ALLOC_FLAG, nullptr, &curve, &size))
    {
        return false;
    }
    const bool p256 = std::strcmp(*reinterpret_cast<LPSTR*>(curve), szOID_ECC_CURVE_P256) == 0;
    LocalFree(curve);
    return p256;
}
}

std::optional<Digest> Sha256(std::span<const uint8_t> data)
{
    Digest digest = {};
    const NTSTATUS status = BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, const_cast<PUCHAR>(data.data()),
        static_cast<ULONG>(data.size()), digest.data(), static_cast<ULONG>(digest.size()));
    return BCRYPT_SUCCESS(status) ? std::optional<Digest>(digest) : std::nullopt;
}

std::optional<std::vector<uint8_t>> RandomBytes(size_t count)
{
    std::vector<uint8_t> bytes(count);
    const NTSTATUS status = BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return BCRYPT_SUCCESS(status) ? std::optional(std::move(bytes)) : std::nullopt;
}

std::optional<std::vector<uint8_t>> CertificateSpki(std::span<const uint8_t> certificateDer)
{
    PCCERT_CONTEXT certificate = CertCreateCertificateContext(X509_ASN_ENCODING, certificateDer.data(),
        static_cast<DWORD>(certificateDer.size()));
    if (!certificate)
    {
        return std::nullopt;
    }

    std::optional<std::vector<uint8_t>> spki;
    DWORD size = 0;
    CERT_PUBLIC_KEY_INFO* info = &certificate->pCertInfo->SubjectPublicKeyInfo;
    if (CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, info, NoFlags, nullptr, nullptr, &size))
    {
        std::vector<uint8_t> encoded(size);
        if (CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, info, NoFlags, nullptr, encoded.data(), &size))
        {
            encoded.resize(size);
            spki = std::move(encoded);
        }
    }
    CertFreeCertificateContext(certificate);
    return spki;
}

std::optional<Digest> SpkiFingerprint(std::span<const uint8_t> spkiDer)
{
    return Sha256(spkiDer);
}

bool IsEcdsaP256Spki(std::span<const uint8_t> spkiDer)
{
    const PublicKeyInfo info = DecodeSpki(spkiDer);
    return info && IsP256(*info);
}

bool ConstantTimeEquals(std::span<const uint8_t> a, std::span<const uint8_t> b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    uint8_t difference = 0;
    for (size_t index = 0; index < a.size(); ++index)
    {
        difference |= static_cast<uint8_t>(a[index] ^ b[index]);
    }
    return difference == 0;
}

bool VerifyEcdsaP256(std::span<const uint8_t> spkiDer, std::span<const uint8_t> message,
    std::span<const uint8_t> rawSignature)
{
    if (rawSignature.size() != EcdsaP256SignatureSize)
    {
        return false;
    }
    const PublicKeyInfo info = DecodeSpki(spkiDer);
    if (!info || !IsP256(*info))
    {
        return false;
    }

    BCRYPT_KEY_HANDLE rawKey = nullptr;
    if (!CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING, info.get(), NoFlags, nullptr, &rawKey))
    {
        return false;
    }
    const KeyHandle key(rawKey);

    const std::optional<Digest> digest = Sha256(message);
    if (!digest)
    {
        return false;
    }
    const NTSTATUS status = BCryptVerifySignature(key.get(), nullptr, const_cast<PUCHAR>(digest->data()),
        static_cast<ULONG>(digest->size()), const_cast<PUCHAR>(rawSignature.data()),
        static_cast<ULONG>(rawSignature.size()), NoFlags);
    return BCRYPT_SUCCESS(status);
}
}
