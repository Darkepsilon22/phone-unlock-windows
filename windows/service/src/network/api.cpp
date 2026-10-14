#include "network/api.h"
#include "network/api_errors.h"
#include "log.h"

namespace
{
constexpr wchar_t PairPath[] = L"/v1/pair";
constexpr wchar_t PairConfirmPath[] = L"/v1/pair/confirm";
constexpr wchar_t StatePath[] = L"/v1/state";
}

Api::Api(PairingManager& pairing, StateEndpoint& state, UnlockEndpoint& unlock)
    : pairing_(pairing), state_(state), unlock_(unlock)
{
}

HttpResponse Api::Handle(const HttpRequest& request)
{
    LogLine(L"https %s %s certificate=%d", request.verb == HttpVerbPOST ? L"POST" : L"GET", request.path.c_str(),
        request.clientFingerprint.has_value());

    if (request.verb == HttpVerbPOST && request.path == PairPath)
    {
        return pairing_.HandlePairRequest(request);
    }
    if (request.verb == HttpVerbPOST && request.path == PairConfirmPath)
    {
        return pairing_.HandlePairConfirm(request);
    }
    if (request.verb == HttpVerbGET && request.path == StatePath)
    {
        return state_.Handle(request);
    }
    if (unlock_.Serves(request))
    {
        return unlock_.Handle(request);
    }
    return api_errors::Error(api_errors::NotFound, "NOT_FOUND");
}
