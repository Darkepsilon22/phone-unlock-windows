#include "ipc/pipe_server.h"
#include "log.h"
#include "pipe_io.h"
#include <sddl.h>

namespace
{
constexpr wchar_t LogonUiPipeSecurity[] = L"D:P(A;;GA;;;SY)(A;;GA;;;OW)";
constexpr wchar_t AdminPipeSecurity[] = L"D:P(A;;GRGW;;;BA)(A;;GA;;;SY)(A;;GA;;;OW)";
constexpr DWORD PipeBufferSize = 8192;
constexpr DWORD ClientCloseTimeout = 5000;

class SecurityAttributes
{
public:
    explicit SecurityAttributes(PCWSTR sddl)
    {
        attributes_.nLength = sizeof(attributes_);
        attributes_.bInheritHandle = FALSE;
        ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1,
            &attributes_.lpSecurityDescriptor, nullptr);
    }

    SecurityAttributes(const SecurityAttributes&) = delete;
    SecurityAttributes& operator=(const SecurityAttributes&) = delete;

    ~SecurityAttributes()
    {
        LocalFree(attributes_.lpSecurityDescriptor);
    }

    SECURITY_ATTRIBUTES* get()
    {
        return attributes_.lpSecurityDescriptor ? &attributes_ : nullptr;
    }

private:
    SECURITY_ATTRIBUTES attributes_ = {};
};

void WaitForClientClose(HANDLE pipe, HANDLE stopEvent)
{
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent)
    {
        return;
    }

    BYTE ignored = 0;
    DWORD transferred = 0;
    if (!ReadFile(pipe, &ignored, sizeof(ignored), &transferred, &overlapped) && GetLastError() == ERROR_IO_PENDING)
    {
        HANDLE handles[] = { overlapped.hEvent, stopEvent };
        if (WaitForMultipleObjects(2, handles, FALSE, ClientCloseTimeout) != WAIT_OBJECT_0)
        {
            CancelIoEx(pipe, &overlapped);
        }
        GetOverlappedResult(pipe, &overlapped, &transferred, TRUE);
    }
    CloseHandle(overlapped.hEvent);
}
}

PipeServer::PipeServer(PipeRole role, ConnectionHandler handler) : role_(role), handler_(std::move(handler))
{
}

void PipeServer::Run(HANDLE stopEvent)
{
    bool first = true;
    while (WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0)
    {
        HANDLE pipe = CreateInstance(first);
        if (pipe == INVALID_HANDLE_VALUE)
        {
            LogLine(L"pipe creation failed error=%lu", GetLastError());
            break;
        }
        first = false;

        if (WaitForClient(pipe, stopEvent))
        {
            StartWorker(pipe, stopEvent);
        }
        else
        {
            CloseHandle(pipe);
        }
    }
    workers_.JoinAll();
}

HANDLE PipeServer::CreateInstance(bool first) const
{
    const bool logonUi = role_ == PipeRole::LogonUi;
    SecurityAttributes security(logonUi ? LogonUiPipeSecurity : AdminPipeSecurity);
    if (!security.get())
    {
        SetLastError(ERROR_INVALID_SECURITY_DESCR);
        return INVALID_HANDLE_VALUE;
    }
    const PCWSTR name = logonUi ? pipe_protocol::LogonUiPipeName : pipe_protocol::AdminPipeName;
    const DWORD openMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0);
    const DWORD pipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
    return CreateNamedPipeW(name, openMode, pipeMode, PIPE_UNLIMITED_INSTANCES,
        PipeBufferSize, PipeBufferSize, 0, security.get());
}

bool PipeServer::WaitForClient(HANDLE pipe, HANDLE stopEvent) const
{
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent)
    {
        return false;
    }

    bool connected = ConnectNamedPipe(pipe, &overlapped) != FALSE;
    const DWORD error = GetLastError();
    if (!connected && error == ERROR_PIPE_CONNECTED)
    {
        connected = true;
    }
    else if (!connected && error == ERROR_IO_PENDING)
    {
        HANDLE handles[] = { overlapped.hEvent, stopEvent };
        DWORD transferred = 0;
        if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0)
        {
            connected = GetOverlappedResult(pipe, &overlapped, &transferred, FALSE) != FALSE;
        }
        else
        {
            CancelIoEx(pipe, &overlapped);
            GetOverlappedResult(pipe, &overlapped, &transferred, TRUE);
        }
    }
    CloseHandle(overlapped.hEvent);
    return connected;
}

void PipeServer::StartWorker(HANDLE pipe, HANDLE stopEvent)
{
    workers_.Start([this, pipe, stopEvent] {
        Serve(pipe, stopEvent);
        WaitForClientClose(pipe, stopEvent);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    });
}

void PipeServer::Serve(HANDLE pipe, HANDLE stopEvent)
{
    std::optional<pipe_protocol::Message> first = pipe_protocol::ReadMessage(pipe, stopEvent);
    if (first)
    {
        handler_(pipe, std::move(*first), stopEvent);
    }
}
