#include "test_support.h"
#include "encoding/text.h"
#include "storage/file_io.h"
#include <wincrypt.h>

namespace test_support
{
namespace
{
constexpr ULONG P256KeyBits = 256;
constexpr size_t DirectorySuffixBytes = 8;
constexpr int Base64BitsPerChar = 6;
constexpr int BitsPerByte = 8;

std::vector<uint8_t> ExportSpki(BCRYPT_KEY_HANDLE handle)
{
    DWORD size = 0;
    CryptExportPublicKeyInfoFromBCryptKeyHandle(handle, X509_ASN_ENCODING, const_cast<LPSTR>(szOID_ECC_PUBLIC_KEY), 0,
        nullptr, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    auto* info = reinterpret_cast<CERT_PUBLIC_KEY_INFO*>(buffer.data());
    CryptExportPublicKeyInfoFromBCryptKeyHandle(handle, X509_ASN_ENCODING, const_cast<LPSTR>(szOID_ECC_PUBLIC_KEY), 0,
        nullptr, info, &size);

    DWORD encodedSize = 0;
    CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, info, 0, nullptr, nullptr, &encodedSize);
    std::vector<uint8_t> spki(encodedSize);
    CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, info, 0, nullptr, spki.data(), &encodedSize);
    spki.resize(encodedSize);
    return spki;
}

int Base64UrlValue(char character)
{
    if (character >= 'A' && character <= 'Z')
    {
        return character - 'A';
    }
    if (character >= 'a' && character <= 'z')
    {
        return character - 'a' + 26;
    }
    if (character >= '0' && character <= '9')
    {
        return character - '0' + 52;
    }
    if (character == '-')
    {
        return 62;
    }
    return character == '_' ? 63 : -1;
}
}

GeneratedKey::GeneratedKey()
{
    BCryptGenerateKeyPair(BCRYPT_ECDSA_P256_ALG_HANDLE, &handle_, P256KeyBits, 0);
    BCryptFinalizeKeyPair(handle_, 0);
    spki_ = ExportSpki(handle_);
}

GeneratedKey::~GeneratedKey()
{
    BCryptDestroyKey(handle_);
}

const std::vector<uint8_t>& GeneratedKey::Spki() const
{
    return spki_;
}

std::vector<uint8_t> GeneratedKey::Sign(std::span<const uint8_t> message) const
{
    const crypto::Digest digest = *crypto::Sha256(message);
    std::vector<uint8_t> signature(crypto::EcdsaP256SignatureSize);
    ULONG size = 0;
    BCryptSignHash(handle_, nullptr, const_cast<PUCHAR>(digest.data()), static_cast<ULONG>(digest.size()),
        signature.data(), static_cast<ULONG>(signature.size()), &size, 0);
    return signature;
}

TemporaryDirectory::TemporaryDirectory()
{
    wchar_t base[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, base);
    path_ = std::wstring(base) + L"phoneunlock-tests-" + HexFromBytes(*crypto::RandomBytes(DirectorySuffixBytes));
    CreateDirectoryW(path_.c_str(), nullptr);
}

TemporaryDirectory::~TemporaryDirectory()
{
    for (const std::wstring& file : ListFiles(path_, L"*"))
    {
        DeleteFileW(file.c_str());
    }
    RemoveDirectoryW(path_.c_str());
}

const std::wstring& TemporaryDirectory::Path() const
{
    return path_;
}

MonotonicClock FakeClock::Function()
{
    return [this] { return now_; };
}

void FakeClock::Advance(ULONGLONG milliseconds)
{
    now_ += milliseconds;
}

std::span<const uint8_t> AsBytes(std::string_view text)
{
    return { reinterpret_cast<const uint8_t*>(text.data()), text.size() };
}

std::optional<std::vector<uint8_t>> Base64UrlDecode(std::string_view text)
{
    std::vector<uint8_t> bytes;
    uint32_t buffer = 0;
    int bits = 0;
    for (char character : text)
    {
        const int value = Base64UrlValue(character);
        if (value < 0)
        {
            return std::nullopt;
        }
        buffer = (buffer << Base64BitsPerChar) | static_cast<uint32_t>(value);
        bits += Base64BitsPerChar;
        if (bits >= BitsPerByte)
        {
            bits -= BitsPerByte;
            bytes.push_back(static_cast<uint8_t>(buffer >> bits));
        }
    }
    return bytes;
}
}
