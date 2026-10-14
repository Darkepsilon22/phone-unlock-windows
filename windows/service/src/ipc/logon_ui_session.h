#pragma once

#include "ipc/logon_tiles.h"
#include "pipe_protocol.h"
#include "storage/secret_store.h"
#include "unlock/unlock_requests.h"
#include <windows.h>

class LogonUiSession
{
public:
    LogonUiSession(const SecretStore& secrets, LogonTiles& tiles, UnlockRequests& requests);

    void Serve(HANDLE pipe, pipe_protocol::Message first, HANDLE stopEvent);

private:
    bool WaitAndSendReady(HANDLE pipe, const pipe_protocol::Message& wait, HANDLE stopEvent);
    void AnswerTileRequest(HANDLE pipe, const pipe_protocol::Message& request, HANDLE stopEvent);
    void CancelTileRequest(HANDLE pipe, const pipe_protocol::Message& request, HANDLE stopEvent);
    bool SendReady(HANDLE pipe, const std::wstring& sid, const SecureString& password, HANDLE stopEvent);

    const SecretStore& secrets_;
    LogonTiles& tiles_;
    UnlockRequests& requests_;
};
