#pragma once

#include "ipc/logon_tiles.h"
#include "pipe_protocol.h"
#include "storage/secret_store.h"
#include <windows.h>
#include <functional>

class LogonUiSession
{
public:
    using ResultHandler = std::function<void(const std::wstring& sid, LONG status)>;

    LogonUiSession(const SecretStore& secrets, LogonTiles& tiles, ResultHandler onResult);

    void Serve(HANDLE pipe, pipe_protocol::Message first, HANDLE stopEvent);

private:
    bool WaitAndSendReady(HANDLE pipe, TileScenario scenario, std::vector<std::wstring> sids, HANDLE stopEvent);
    bool SendReady(HANDLE pipe, const std::wstring& sid, const SecureString& password, HANDLE stopEvent);

    const SecretStore& secrets_;
    LogonTiles& tiles_;
    ResultHandler onResult_;
};
