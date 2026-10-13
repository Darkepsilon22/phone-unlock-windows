#pragma once

#include "pipe_protocol.h"
#include "support/worker_group.h"
#include <windows.h>
#include <functional>

enum class PipeRole
{
    LogonUi,
    Administrator,
};

class PipeServer
{
public:
    using ConnectionHandler = std::function<void(HANDLE pipe, pipe_protocol::Message first, HANDLE stopEvent)>;

    PipeServer(PipeRole role, ConnectionHandler handler);

    void Run(HANDLE stopEvent);

private:
    HANDLE CreateInstance(bool first) const;
    bool WaitForClient(HANDLE pipe, HANDLE stopEvent) const;
    void StartWorker(HANDLE pipe, HANDLE stopEvent);
    void Serve(HANDLE pipe, HANDLE stopEvent);

    PipeRole role_;
    ConnectionHandler handler_;
    WorkerGroup workers_;
};
