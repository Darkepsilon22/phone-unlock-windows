#include "pairing/pairing_protocol.h"
#include "encoding/text.h"
#include <algorithm>
#include <charconv>

namespace pairing_protocol
{
namespace
{
constexpr char QrPrefix[] = "phoneunlock:1:";
constexpr char TranscriptDomain[] = "PhoneUnlock-v1-pair";
constexpr char ConfirmationDomain[] = "PhoneUnlock-v1-pair-confirm";
constexpr uint32_t CodeModulus = 1'000'000;
constexpr size_t MaxHostLength = 253;

enum QrField : uint64_t
{
    VersionField = 1,
    PcIdField = 2,
    FingerprintField = 3,
    TokenField = 4,
    EndpointsField = 5,
};

void AppendDomain(cbor::Bytes& out, const char* domain)
{
    out.insert(out.end(), domain, domain + std::char_traits<char>::length(domain));
    out.push_back(0);
}

template <typename Bytes>
void Append(cbor::Bytes& out, const Bytes& bytes)
{
    out.insert(out.end(), bytes.begin(), bytes.end());
}

bool IsHostCharacter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-';
}
}

std::optional<Endpoint> ParseEndpoint(const std::string& text)
{
    const size_t separator = text.rfind(':');
    const std::string host = separator == std::string::npos ? text : text.substr(0, separator);
    if (host.empty() || host.size() > MaxHostLength || !std::all_of(host.begin(), host.end(), IsHostCharacter))
    {
        return std::nullopt;
    }

    Endpoint endpoint{ host, DefaultPort };
    if (separator != std::string::npos)
    {
        const char* first = text.data() + separator + 1;
        const char* last = text.data() + text.size();
        const auto [end, error] = std::from_chars(first, last, endpoint.port);
        if (error != std::errc() || end != last || endpoint.port == 0)
        {
            return std::nullopt;
        }
    }
    return endpoint;
}

std::string QrText(const cbor::Bytes& pcId, const crypto::Digest& pcFingerprint, const cbor::Bytes& token,
    const std::vector<Endpoint>& endpoints)
{
    cbor::Array endpointList;
    for (const Endpoint& endpoint : endpoints)
    {
        endpointList.push_back(cbor::List({ cbor::Text(endpoint.host), cbor::UInt(endpoint.port) }));
    }
    const cbor::Bytes encoded = cbor::Encode(cbor::Object({
        { VersionField, cbor::UInt(ProtocolVersion) },
        { PcIdField, cbor::ByteString(pcId) },
        { FingerprintField, cbor::ByteString({ pcFingerprint.begin(), pcFingerprint.end() }) },
        { TokenField, cbor::ByteString(token) },
        { EndpointsField, cbor::List(std::move(endpointList)) },
    }));
    return QrPrefix + Base64Url(encoded);
}

std::optional<crypto::Digest> Transcript(const cbor::Bytes& pcId, const crypto::Digest& pcFingerprint,
    const crypto::Digest& transportFingerprint, const crypto::Digest& authorizationFingerprint,
    const cbor::Bytes& token, const cbor::Bytes& pairingId)
{
    cbor::Bytes input;
    AppendDomain(input, TranscriptDomain);
    Append(input, pcId);
    Append(input, pcFingerprint);
    Append(input, transportFingerprint);
    Append(input, authorizationFingerprint);
    Append(input, token);
    Append(input, pairingId);
    return crypto::Sha256(input);
}

std::wstring ComparisonCode(const crypto::Digest& transcript)
{
    const uint32_t prefix = (static_cast<uint32_t>(transcript[0]) << 24) | (static_cast<uint32_t>(transcript[1]) << 16) |
        (static_cast<uint32_t>(transcript[2]) << 8) | transcript[3];
    wchar_t code[7] = {};
    swprintf_s(code, L"%06u", prefix % CodeModulus);
    return code;
}

cbor::Bytes ConfirmationMessage(const crypto::Digest& transcript)
{
    cbor::Bytes message;
    AppendDomain(message, ConfirmationDomain);
    Append(message, transcript);
    return message;
}
}
