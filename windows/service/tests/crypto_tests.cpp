#include "crypto/crypto.h"
#include "test_runner.h"
#include "test_support.h"

namespace
{
using test_support::AsBytes;
using test_support::GeneratedKey;

constexpr std::string_view Abc = "abc";

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
    CHECK(crypto::IsEcdsaP256Spki(key.Spki()));
    CHECK(crypto::VerifyEcdsaP256(key.Spki(), AsBytes(Abc), signature));
}

TEST(RejectsTamperedMessageOrSignature)
{
    const GeneratedKey key;
    std::vector<uint8_t> signature = key.Sign(AsBytes(Abc));
    CHECK(!crypto::VerifyEcdsaP256(key.Spki(), AsBytes("abd"), signature));
    signature[10] ^= 0x01;
    CHECK(!crypto::VerifyEcdsaP256(key.Spki(), AsBytes(Abc), signature));
    CHECK(!crypto::VerifyEcdsaP256(key.Spki(), AsBytes(Abc), std::span<const uint8_t>(signature).first(63)));
}

TEST(RejectsSignatureFromAnotherKey)
{
    const GeneratedKey signer;
    const GeneratedKey other;
    CHECK(!crypto::VerifyEcdsaP256(other.Spki(), AsBytes(Abc), signer.Sign(AsBytes(Abc))));
}

TEST(RejectsNonP256PublicKey)
{
    CHECK(!crypto::IsEcdsaP256Spki(AsBytes("not a key")));
}

TEST(ConstantTimeEqualsComparesContentAndLength)
{
    CHECK(crypto::ConstantTimeEquals(AsBytes("abc"), AsBytes("abc")));
    CHECK(!crypto::ConstantTimeEquals(AsBytes("abc"), AsBytes("abd")));
    CHECK(!crypto::ConstantTimeEquals(AsBytes("abc"), AsBytes("ab")));
}
}
