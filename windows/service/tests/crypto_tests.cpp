#include "crypto/crypto.h"
#include "test_runner.h"
#include <bcrypt.h>
#include <wincrypt.h>
#include <string_view>

namespace
{
constexpr std::string_view Abc = "abc";

struct GeneratedKey
{
    BCRYPT_KEY_HANDLE handle = nullptr;
    std::vector<uint8_t> spki;

    GeneratedKey()
    {
        BCryptGenerateKeyPair(BCRYPT_ECDSA_P256_ALG_HANDLE, &handle, 256, 0);
        BCryptFinalizeKeyPair(handle, 0);

        DWORD size = 0;
        CryptExportPublicKeyInfoFromBCryptKeyHandle(handle, X509_ASN_ENCODING, const_cast<LPSTR>(szOID_ECC_PUBLIC_KEY),
            0, nullptr, nullptr, &size);
        std::vector<uint8_t> buffer(size);
        auto* info = reinterpret_cast<CERT_PUBLIC_KEY_INFO*>(buffer.data());
        CryptExportPublicKeyInfoFromBCryptKeyHandle(handle, X509_ASN_ENCODING, const_cast<LPSTR>(szOID_ECC_PUBLIC_KEY),
            0, nullptr, info, &size);

        DWORD encodedSize = 0;
        CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, info, 0, nullptr, nullptr, &encodedSize);
        spki.resize(encodedSize);
        CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, info, 0, nullptr, spki.data(), &encodedSize);
        spki.resize(encodedSize);
    }

    ~GeneratedKey()
    {
        BCryptDestroyKey(handle);
    }

    std::vector<uint8_t> Sign(std::span<const uint8_t> message) const
    {
        const crypto::Digest digest = *crypto::Sha256(message);
        std::vector<uint8_t> signature(crypto::EcdsaP256SignatureSize);
        ULONG size = 0;
        BCryptSignHash(handle, nullptr, const_cast<PUCHAR>(digest.data()), static_cast<ULONG>(digest.size()),
            signature.data(), static_cast<ULONG>(signature.size()), &size, 0);
        return signature;
    }
};

std::span<const uint8_t> AsBytes(std::string_view text)
{
    return { reinterpret_cast<const uint8_t*>(text.data()), text.size() };
}

TEST(Sha256MatchesKnownVector)
{
    const crypto::Digest digest = *crypto::Sha256(AsBytes(Abc));
    CHECK(digest[0] == 0xBA && digest[1] == 0x78 && digest[30] == 0x15 && digest[31] == 0xAD);
}

TEST(RandomBytesHaveRequestedSize)
{
    CHECK(crypto::RandomBytes(32)->size() == 32);
}

TEST(VerifiesValidP256Signature)
{
    const GeneratedKey key;
    const std::vector<uint8_t> signature = key.Sign(AsBytes(Abc));
    CHECK(crypto::VerifyEcdsaP256(key.spki, AsBytes(Abc), signature));
}

TEST(RejectsTamperedMessageOrSignature)
{
    const GeneratedKey key;
    std::vector<uint8_t> signature = key.Sign(AsBytes(Abc));
    CHECK(!crypto::VerifyEcdsaP256(key.spki, AsBytes("abd"), signature));
    signature[10] ^= 0x01;
    CHECK(!crypto::VerifyEcdsaP256(key.spki, AsBytes(Abc), signature));
    CHECK(!crypto::VerifyEcdsaP256(key.spki, AsBytes(Abc), std::span<const uint8_t>(signature).first(63)));
}
}
