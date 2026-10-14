#include "network/state_endpoint.h"
#include "network/api_errors.h"
#include <optional>

namespace
{
constexpr wchar_t SinceParameter[] = L"?since=";

enum StateField : uint64_t
{
    VersionField = 1,
    SessionField = 2,
    UnlockAvailableField = 3,
    PcNameField = 4,
};

std::optional<uint64_t> SinceVersion(const std::wstring& query)
{
    if (query.empty())
    {
        return 0;
    }
    const std::wstring prefix(SinceParameter);
    if (!query.starts_with(prefix) || query.size() == prefix.size())
    {
        return std::nullopt;
    }
    uint64_t version = 0;
    for (size_t index = prefix.size(); index < query.size(); ++index)
    {
        const wchar_t digit = query[index];
        if (digit < L'0' || digit > L'9' || version > (UINT64_MAX - 9) / 10)
        {
            return std::nullopt;
        }
        version = version * 10 + static_cast<uint64_t>(digit - L'0');
    }
    return version;
}
}

StateEndpoint::StateEndpoint(PairingStore& pairings, ChangeFeed& changes, Diagnostics& diagnostics, ViewProvider views,
    std::string pcName, std::chrono::milliseconds longPoll)
    : pairings_(pairings),
      changes_(changes),
      diagnostics_(diagnostics),
      views_(std::move(views)),
      pcName_(std::move(pcName)),
      longPoll_(longPoll)
{
}

HttpResponse StateEndpoint::Handle(const HttpRequest& request)
{
    const std::optional<Pairing> pairing =
        request.clientFingerprint ? pairings_.FindByTransport(*request.clientFingerprint) : std::nullopt;
    if (!pairing)
    {
        diagnostics_.RecordError({}, "REVOKED");
        return api_errors::Error(api_errors::Forbidden, "REVOKED");
    }
    const std::optional<uint64_t> since = SinceVersion(request.query);
    if (!since)
    {
        return api_errors::Error(api_errors::BadRequest, "INVALID_PROOF");
    }

    const uint64_t version = changes_.WaitForChange(*since, longPoll_);
    const AccountView view = views_(pairing->accountSid);
    return api_errors::Body(cbor::Object({
        { VersionField, cbor::UInt(version) },
        { SessionField, cbor::Text(SessionStateText(view.state)) },
        { UnlockAvailableField, cbor::Boolean(view.unlockAvailable) },
        { PcNameField, cbor::Text(pcName_) },
    }));
}
