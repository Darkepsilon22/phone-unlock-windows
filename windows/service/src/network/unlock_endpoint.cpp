#include "network/unlock_endpoint.h"
#include "network/api_errors.h"
#include <optional>

namespace
{
constexpr wchar_t UnlockPath[] = L"/v1/unlock";
constexpr wchar_t RequestsPrefix[] = L"/v1/requests/";
constexpr wchar_t ProofSuffix[] = L"/proof";
constexpr wchar_t ResultSuffix[] = L"/result";
constexpr size_t RequestIdHexLength = unlock_protocol::RequestIdSize * 2;

std::optional<int> HexValue(wchar_t digit)
{
    if (digit >= L'0' && digit <= L'9')
    {
        return digit - L'0';
    }
    if (digit >= L'a' && digit <= L'f')
    {
        return digit - L'a' + 10;
    }
    return std::nullopt;
}

std::optional<cbor::Bytes> RequestIdIn(const std::wstring& path, const wchar_t* suffix)
{
    const std::wstring prefix(RequestsPrefix);
    const std::wstring end(suffix);
    if (path.size() != prefix.size() + RequestIdHexLength + end.size() || !path.starts_with(prefix) ||
        !path.ends_with(end))
    {
        return std::nullopt;
    }
    cbor::Bytes id;
    for (size_t index = prefix.size(); index < prefix.size() + RequestIdHexLength; index += 2)
    {
        const std::optional<int> high = HexValue(path[index]);
        const std::optional<int> low = HexValue(path[index + 1]);
        if (!high || !low)
        {
            return std::nullopt;
        }
        id.push_back(static_cast<uint8_t>((*high << 4) | *low));
    }
    return id;
}
}

UnlockEndpoint::UnlockEndpoint(PairingStore& pairings, UnlockRequests& requests, std::chrono::milliseconds resultWait)
    : pairings_(pairings), requests_(requests), resultWait_(resultWait)
{
}

bool UnlockEndpoint::Serves(const HttpRequest& request) const
{
    return request.path == UnlockPath || request.path.starts_with(RequestsPrefix);
}

HttpResponse UnlockEndpoint::Handle(const HttpRequest& request)
{
    const std::optional<Pairing> pairing =
        request.clientFingerprint ? pairings_.FindByTransport(*request.clientFingerprint) : std::nullopt;
    if (!pairing)
    {
        return api_errors::Error(api_errors::Forbidden, "REVOKED");
    }
    if (!request.query.empty())
    {
        return api_errors::Error(api_errors::BadRequest, "INVALID_PROOF");
    }

    if (request.verb == HttpVerbPOST && request.path == UnlockPath)
    {
        return request.body.empty() ? requests_.Create(*pairing) : api_errors::Error(api_errors::BadRequest, "INVALID_PROOF");
    }
    if (request.verb == HttpVerbPOST)
    {
        if (const std::optional<cbor::Bytes> id = RequestIdIn(request.path, ProofSuffix))
        {
            return requests_.Prove(*pairing, *id, request.body);
        }
    }
    if (request.verb == HttpVerbGET)
    {
        if (const std::optional<cbor::Bytes> id = RequestIdIn(request.path, ResultSuffix))
        {
            return requests_.Result(*pairing, *id, resultWait_);
        }
    }
    return api_errors::Error(api_errors::NotFound, "NOT_FOUND");
}
