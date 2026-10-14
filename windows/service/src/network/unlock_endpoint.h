#pragma once

#include "network/http_server.h"
#include "storage/pairing_store.h"
#include "support/diagnostics.h"
#include "unlock/unlock_requests.h"
#include <chrono>

class UnlockEndpoint
{
public:
    UnlockEndpoint(PairingStore& pairings, UnlockRequests& requests, Diagnostics& diagnostics,
        std::chrono::milliseconds resultWait);

    bool Serves(const HttpRequest& request) const;
    HttpResponse Handle(const HttpRequest& request);

private:
    PairingStore& pairings_;
    UnlockRequests& requests_;
    Diagnostics& diagnostics_;
    std::chrono::milliseconds resultWait_;
};
