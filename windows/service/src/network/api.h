#pragma once

#include "network/http_server.h"
#include "pairing/pairing_manager.h"

class Api
{
public:
    explicit Api(PairingManager& pairing);

    HttpResponse Handle(const HttpRequest& request);

private:
    PairingManager& pairing_;
};
