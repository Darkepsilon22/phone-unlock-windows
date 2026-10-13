#pragma once

#include "authorization_broker.h"
#include "pipe_protocol.h"
#include "secret_store.h"
#include <atomic>
#include <list>
#include <memory>
#include <thread>

enum class PipeRole
{
    LogonUi,
    Administrator,
};

class PipeServer
{
public:
    PipeServer(PipeRole role, const SecretStore& secrets, AuthorizationBroker& broker);

    void Run(HANDLE stopEvent);

private:
    struct Worker
    {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> finished;
    };

    HANDLE CreateInstance(bool first) const;
    bool WaitForClient(HANDLE pipe, HANDLE stopEvent) const;
    void StartWorker(HANDLE pipe, HANDLE stopEvent);
    void JoinFinishedWorkers();
    void JoinAllWorkers();

    void Serve(HANDLE pipe, HANDLE stopEvent);
    void ServeLogonUi(HANDLE pipe, pipe_protocol::Message message, HANDLE stopEvent);
    void ServeAdministrator(HANDLE pipe, const pipe_protocol::Message& message, HANDLE stopEvent);
    bool WaitAndSendReady(HANDLE pipe, const std::vector<std::wstring>& sids, HANDLE stopEvent);
    bool SendReady(HANDLE pipe, const std::wstring& sid, const SecureString& password, HANDLE stopEvent);

    PipeRole role_;
    const SecretStore& secrets_;
    AuthorizationBroker& broker_;
    std::list<Worker> workers_;
};
