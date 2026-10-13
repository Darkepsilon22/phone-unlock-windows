#pragma once

#include "ipc/authorization_broker.h"
#include "pipe_protocol.h"
#include "storage/secret_store.h"
#include <windows.h>

class LogonUiSession
{
public:
    LogonUiSession(const SecretStore& secrets, AuthorizationBroker& broker);

    void Serve(HANDLE pipe, pipe_protocol::Message first, HANDLE stopEvent);

private:
    bool WaitAndSendReady(HANDLE pipe, const std::vector<std::wstring>& sids, HANDLE stopEvent);
    bool SendReady(HANDLE pipe, const std::wstring& sid, const SecureString& password, HANDLE stopEvent);

    const SecretStore& secrets_;
    AuthorizationBroker& broker_;
};
