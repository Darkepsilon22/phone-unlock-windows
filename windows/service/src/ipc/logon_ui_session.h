#pragma once

#include "ipc/logon_tiles.h"
#include "pipe_protocol.h"
#include "storage/secret_store.h"
#include <windows.h>

class LogonUiSession
{
public:
    LogonUiSession(const SecretStore& secrets, LogonTiles& tiles);

    void Serve(HANDLE pipe, pipe_protocol::Message first, HANDLE stopEvent);

private:
    bool WaitAndSendReady(HANDLE pipe, TileScenario scenario, std::vector<std::wstring> sids, HANDLE stopEvent);
    bool SendReady(HANDLE pipe, const std::wstring& sid, const SecureString& password, HANDLE stopEvent);

    const SecretStore& secrets_;
    LogonTiles& tiles_;
};
