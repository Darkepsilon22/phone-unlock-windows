#pragma once

#include "network/http_server.h"
#include "session/pc_state.h"
#include "storage/pairing_store.h"
#include "support/change_feed.h"
#include "support/diagnostics.h"
#include <chrono>
#include <functional>
#include <string>

class StateEndpoint
{
public:
    using ViewProvider = std::function<AccountView(const std::wstring& sid)>;

    StateEndpoint(PairingStore& pairings, ChangeFeed& changes, Diagnostics& diagnostics, ViewProvider views,
        std::string pcName, std::chrono::milliseconds longPoll);

    HttpResponse Handle(const HttpRequest& request);

private:
    PairingStore& pairings_;
    ChangeFeed& changes_;
    Diagnostics& diagnostics_;
    ViewProvider views_;
    std::string pcName_;
    std::chrono::milliseconds longPoll_;
};
