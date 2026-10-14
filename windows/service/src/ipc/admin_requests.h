#pragma once

#include "ipc/logon_tiles.h"
#include "pairing/pairing_manager.h"
#include "pipe_protocol.h"
#include "storage/secret_store.h"
#include "support/diagnostics.h"

class AdminRequests
{
public:
    AdminRequests(const SecretStore& secrets, LogonTiles& tiles, PairingStore& pairings, Diagnostics& diagnostics,
        PairingManager* pairing);

    pipe_protocol::PayloadWriter Handle(const pipe_protocol::Message& request);

private:
    pipe_protocol::PayloadWriter Provision(const pipe_protocol::Message& request);
    pipe_protocol::PayloadWriter Authorize(const pipe_protocol::Message& request);
    pipe_protocol::PayloadWriter StartPairing(const pipe_protocol::Message& request);
    pipe_protocol::PayloadWriter QueryPairingStatus();
    pipe_protocol::PayloadWriter ConfirmPairing(const pipe_protocol::Message& request);
    pipe_protocol::PayloadWriter ListPhones();
    pipe_protocol::PayloadWriter RevokePhone(const pipe_protocol::Message& request);
    pipe_protocol::PayloadWriter Status(const pipe_protocol::Message& request);
    pipe_protocol::PayloadWriter ClearHistory(const pipe_protocol::Message& request);

    const SecretStore& secrets_;
    LogonTiles& tiles_;
    PairingStore& pairings_;
    Diagnostics& diagnostics_;
    PairingManager* pairing_;
};
