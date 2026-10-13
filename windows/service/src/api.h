#pragma once

#include "http_server.h"
#include "pairing_manager.h"

class Api
{
public:
    explicit Api(PairingManager& pairing);

    HttpResponse Handle(const HttpRequest& request);

private:
    PairingManager& pairing_;
};
