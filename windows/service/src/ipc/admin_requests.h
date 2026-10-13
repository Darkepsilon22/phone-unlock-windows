#pragma once

#include "ipc/authorization_broker.h"
#include "pairing/pairing_manager.h"
#include "pipe_protocol.h"
#include "storage/secret_store.h"

class AdminRequests
{
public:
    AdminRequests(const SecretStore& secrets, AuthorizationBroker& broker, PairingManager* pairing);

    pipe_protocol::PayloadWriter Handle(const pipe_protocol::Message& request);

private:
    pipe_protocol::PayloadWriter Provision(const pipe_protocol::Message& request);
    pipe_protocol::PayloadWriter Authorize(const pipe_protocol::Message& request);
    pipe_protocol::PayloadWriter StartPairing(const pipe_protocol::Message& request);
    pipe_protocol::PayloadWriter QueryPairingStatus();
    pipe_protocol::PayloadWriter ConfirmPairing(const pipe_protocol::Message& request);

    const SecretStore& secrets_;
    AuthorizationBroker& broker_;
    PairingManager* pairing_;
};
