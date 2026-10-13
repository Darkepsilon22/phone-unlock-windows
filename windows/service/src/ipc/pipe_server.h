#pragma once

#include "ipc/authorization_broker.h"
#include "pairing/pairing_manager.h"
#include "pipe_protocol.h"
#include "storage/secret_store.h"
#include "support/worker_group.h"

enum class PipeRole
{
    LogonUi,
    Administrator,
};

class PipeServer
{
public:
    PipeServer(PipeRole role, const SecretStore& secrets, AuthorizationBroker& broker, PairingManager* pairing);

    void Run(HANDLE stopEvent);

private:
    HANDLE CreateInstance(bool first) const;
    bool WaitForClient(HANDLE pipe, HANDLE stopEvent) const;
    void StartWorker(HANDLE pipe, HANDLE stopEvent);

    void Serve(HANDLE pipe, HANDLE stopEvent);
    void ServeLogonUi(HANDLE pipe, pipe_protocol::Message message, HANDLE stopEvent);
    void ServeAdministrator(HANDLE pipe, const pipe_protocol::Message& message, HANDLE stopEvent);
    bool WaitAndSendReady(HANDLE pipe, const std::vector<std::wstring>& sids, HANDLE stopEvent);
    bool SendReady(HANDLE pipe, const std::wstring& sid, const SecureString& password, HANDLE stopEvent);

    pipe_protocol::PayloadWriter HandleProvision(const pipe_protocol::Message& message);
    pipe_protocol::PayloadWriter HandleAuthorize(const pipe_protocol::Message& message);
    pipe_protocol::PayloadWriter HandleStartPairing(const pipe_protocol::Message& message);
    pipe_protocol::PayloadWriter HandlePairingStatus();
    pipe_protocol::PayloadWriter HandleConfirmPairing(const pipe_protocol::Message& message);

    PipeRole role_;
    const SecretStore& secrets_;
    AuthorizationBroker& broker_;
    PairingManager* pairing_;
    WorkerGroup workers_;
};
