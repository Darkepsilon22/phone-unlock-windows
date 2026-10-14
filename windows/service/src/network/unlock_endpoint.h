#pragma once

#include "network/http_server.h"
#include "storage/pairing_store.h"
#include "unlock/unlock_requests.h"
#include <chrono>

class UnlockEndpoint
{
public:
    UnlockEndpoint(PairingStore& pairings, UnlockRequests& requests, std::chrono::milliseconds resultWait);

    bool Serves(const HttpRequest& request) const;
    HttpResponse Handle(const HttpRequest& request);

private:
    PairingStore& pairings_;
    UnlockRequests& requests_;
    std::chrono::milliseconds resultWait_;
};
