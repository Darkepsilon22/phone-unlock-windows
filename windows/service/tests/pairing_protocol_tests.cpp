#include "pairing/pairing_protocol.h"
#include "encoding/text.h"
#include "test_runner.h"
#include <string_view>

namespace
{
using pairing_protocol::ParseEndpoint;

crypto::Digest Filled(uint8_t value)
{
    crypto::Digest digest;
    digest.fill(value);
    return digest;
}

TEST(QrTextMatchesReferenceVector)
{
    const std::string text = pairing_protocol::QrText(cbor::Bytes(16, 0x11), Filled(0), cbor::Bytes(32, 0x22),
        { { "10.0.0.2", 47863 } });
    CHECK(text == "phoneunlock:1:pQEBAlARERERERERERERERERERERA1ggAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEWCAi"
                  "IiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIgWBgmgxMC4wLjAuMhm69w");
}

TEST(TranscriptAndCodeMatchReferenceVector)
{
    const crypto::Digest transcript = *pairing_protocol::Transcript(cbor::Bytes(16, 1), Filled(2), Filled(3), Filled(4),
        cbor::Bytes(32, 5), cbor::Bytes(16, 6));
    CHECK(HexFromBytes(transcript) == L"a5525aa572b00afb421f320c95677ee35dca2645caeae64b4d7473bd6761e803");
    CHECK(pairing_protocol::ComparisonCode(transcript) == L"637797");
}

TEST(ParsesEndpointWithAndWithoutPort)
{
    CHECK(ParseEndpoint("192.168.88.159:47863")->host == "192.168.88.159");
    CHECK(ParseEndpoint("192.168.88.159:47863")->port == 47863);
    CHECK(ParseEndpoint("pc-salon")->port == pairing_protocol::DefaultPort);
}

TEST(RejectsMalformedEndpoints)
{
    CHECK(!ParseEndpoint("").has_value());
    CHECK(!ParseEndpoint("host:0").has_value());
    CHECK(!ParseEndpoint("host:99999").has_value());
    CHECK(!ParseEndpoint("bad host:1").has_value());
    CHECK(!ParseEndpoint("host:12a").has_value());
}

TEST(ComparisonCodeKeepsLeadingZeros)
{
    crypto::Digest digest = {};
    CHECK(pairing_protocol::ComparisonCode(digest) == L"000000");
    digest[0] = 0xFF;
    digest[1] = 0xFF;
    digest[2] = 0xFF;
    digest[3] = 0xFF;
    CHECK(pairing_protocol::ComparisonCode(digest) == L"967295");
}

TEST(QrTextHasPrefixAndDecodes)
{
    const cbor::Bytes pcId(16, 0x11);
    const cbor::Bytes token(32, 0x22);
    const crypto::Digest fingerprint = {};
    const std::string text = pairing_protocol::QrText(pcId, fingerprint, token, { { "10.0.0.2", 47863 } });
    CHECK(text.starts_with("phoneunlock:1:"));
    CHECK(text.find('=') == std::string::npos);
    CHECK(text.find('+') == std::string::npos);
}

TEST(TranscriptDependsOnEveryInput)
{
    const cbor::Bytes pcId(16, 1);
    const cbor::Bytes token(32, 2);
    const cbor::Bytes pairingId(16, 3);
    const crypto::Digest a = {};
    crypto::Digest b = {};
    b[0] = 1;
    const crypto::Digest base = *pairing_protocol::Transcript(pcId, a, a, a, token, pairingId);
    CHECK(base != *pairing_protocol::Transcript(pcId, b, a, a, token, pairingId));
    CHECK(base != *pairing_protocol::Transcript(pcId, a, b, a, token, pairingId));
    CHECK(base != *pairing_protocol::Transcript(pcId, a, a, b, token, pairingId));
}

TEST(ConfirmationMessageIsDomainSeparated)
{
    const crypto::Digest transcript = {};
    const cbor::Bytes message = pairing_protocol::ConfirmationMessage(transcript);
    constexpr std::string_view Domain = "PhoneUnlock-v1-pair-confirm";
    CHECK(message.size() == Domain.size() + 1 + transcript.size());
    CHECK(std::string_view(reinterpret_cast<const char*>(message.data()), Domain.size()) == Domain);
    CHECK(message[Domain.size()] == 0);
}
}
