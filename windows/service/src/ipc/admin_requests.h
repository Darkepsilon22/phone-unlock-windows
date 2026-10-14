#pragma once

#include "ipc/logon_tiles.h"
#include "pairing/pairing_manager.h"
#include "pipe_protocol.h"
#include "storage/secret_store.h"

class AdminRequests
{
public:
    AdminRequests(const SecretStore& secrets, LogonTiles& tiles, PairingManager* pairing);

    pipe_protocol::PayloadWriter Handle(const pipe_protocol::Message& request);

private:
    pipe_protocol::PayloadWriter Provision(const pipe_protocol::Message& request);
    pipe_protocol::PayloadWriter Authorize(const pipe_protocol::Message& request);
    pipe_protocol::PayloadWriter StartPairing(const pipe_protocol::Message& request);
    pipe_protocol::PayloadWriter QueryPairingStatus();
    pipe_protocol::PayloadWriter ConfirmPairing(const pipe_protocol::Message& request);

    const SecretStore& secrets_;
    LogonTiles& tiles_;
    PairingManager* pairing_;
};
