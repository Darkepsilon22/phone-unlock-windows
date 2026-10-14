#pragma once

#include "network/http_server.h"
#include "network/state_endpoint.h"
#include "pairing/pairing_manager.h"

class Api
{
public:
    Api(PairingManager& pairing, StateEndpoint& state);

    HttpResponse Handle(const HttpRequest& request);

private:
    PairingManager& pairing_;
    StateEndpoint& state_;
};
