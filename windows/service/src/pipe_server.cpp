#include "pipe_server.h"
#include "log.h"
#include "pipe_io.h"
#include "text.h"
#include <sddl.h>

using pipe_protocol::Message;
using pipe_protocol::MessageType;
using pipe_protocol::PayloadReader;
using pipe_protocol::PayloadWriter;

namespace
{
constexpr wchar_t LogonUiPipeSecurity[] = L"D:P(A;;GA;;;SY)(A;;GA;;;OW)";
constexpr wchar_t AdminPipeSecurity[] = L"D:P(A;;GRGW;;;BA)(A;;GA;;;SY)(A;;GA;;;OW)";
constexpr DWORD PipeBufferSize = 8192;

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

bool IsConnected(HANDLE pipe)
{
    DWORD available = 0;
    return PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) != FALSE;
}

PayloadWriter ResultOnly(HRESULT hr)
{
    PayloadWriter writer;
    writer.WriteInt32(hr);
    return writer;
}

void LogResult(const Message& message)
{
    PayloadReader reader(message.payload);
    std::wstring sid;
    int32_t status = 0;
    int32_t subStatus = 0;
    if (reader.ReadString(&sid) && reader.ReadInt32(&status) && reader.ReadInt32(&subStatus))
    {
        LogLine(L"logon result status=0x%08X substatus=0x%08X", static_cast<unsigned>(status),
            static_cast<unsigned>(subStatus));
    }
}
}

PipeServer::PipeServer(PipeRole role, const SecretStore& secrets, AuthorizationBroker& broker,
    PairingManager* pairing)
    : role_(role), secrets_(secrets), broker_(broker), pairing_(pairing)
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
    broker_.Stop();
    workers_.JoinAll();
}

HANDLE PipeServer::CreateInstance(bool first) const
{
    const bool logonUi = role_ == PipeRole::LogonUi;
    SecurityAttributes security(logonUi ? LogonUiPipeSecurity : AdminPipeSecurity);
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
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    });
}

void PipeServer::Serve(HANDLE pipe, HANDLE stopEvent)
{
    std::optional<Message> first = pipe_protocol::ReadMessage(pipe, stopEvent);
    if (!first)
    {
        return;
    }

    if (role_ == PipeRole::LogonUi)
    {
        LogLine(L"logonui connected");
        ServeLogonUi(pipe, std::move(*first), stopEvent);
    }
    else
    {
        LogLine(L"administrator connected");
        ServeAdministrator(pipe, *first, stopEvent);
    }
}

void PipeServer::ServeLogonUi(HANDLE pipe, Message message, HANDLE stopEvent)
{
    for (;;)
    {
        if (message.type == MessageType::Result)
        {
            LogResult(message);
        }
        else if (message.type == MessageType::Wait)
        {
            std::vector<std::wstring> sids;
            PayloadReader reader(message.payload);
            if (!reader.ReadStringList(&sids) || !reader.AtEnd() || !WaitAndSendReady(pipe, sids, stopEvent))
            {
                return;
            }
        }
        else
        {
            return;
        }

        std::optional<Message> next = pipe_protocol::ReadMessage(pipe, stopEvent);
        if (!next)
        {
            return;
        }
        message = std::move(*next);
    }
}

bool PipeServer::WaitAndSendReady(HANDLE pipe, const std::vector<std::wstring>& sids, HANDLE stopEvent)
{
    for (;;)
    {
        const std::optional<std::wstring> sid =
            broker_.WaitForAuthorization(sids, [pipe] { return IsConnected(pipe); });
        if (!sid)
        {
            return false;
        }

        SecureString password;
        const HRESULT hr = secrets_.Load(*sid, &password);
        if (SUCCEEDED(hr))
        {
            return SendReady(pipe, *sid, password, stopEvent);
        }
        LogLine(L"secret unavailable hr=0x%08X", static_cast<unsigned>(hr));
    }
}

bool PipeServer::SendReady(HANDLE pipe, const std::wstring& sid, const SecureString& password, HANDLE stopEvent)
{
    PayloadWriter writer;
    const bool sent = writer.WriteString(sid) && writer.WriteString(password.c_str(), password.length()) &&
        pipe_protocol::WriteMessage(pipe, MessageType::Ready, writer.Bytes(), stopEvent);
    LogLine(L"ready sent=%d", sent);
    return sent;
}

void PipeServer::ServeAdministrator(HANDLE pipe, const Message& message, HANDLE stopEvent)
{
    PayloadWriter reply;
    switch (message.type)
    {
    case MessageType::Provision:
        reply = HandleProvision(message);
        break;
    case MessageType::Authorize:
        reply = HandleAuthorize(message);
        break;
    case MessageType::StartPairing:
        reply = HandleStartPairing(message);
        break;
    case MessageType::PairingStatus:
        reply = HandlePairingStatus();
        break;
    case MessageType::ConfirmPairing:
        reply = HandleConfirmPairing(message);
        break;
    default:
        reply.WriteInt32(E_INVALIDARG);
        break;
    }
    pipe_protocol::WriteMessage(pipe, MessageType::Ack, reply.Bytes(), stopEvent);
}

PayloadWriter PipeServer::HandleProvision(const Message& message)
{
    PayloadReader reader(message.payload);
    std::wstring sid;
    SecureString password;
    HRESULT hr = E_INVALIDARG;
    if (reader.ReadString(&sid) && reader.ReadSecret(&password) && reader.AtEnd())
    {
        hr = secrets_.Save(sid, password);
    }
    LogLine(L"provision hr=0x%08X", static_cast<unsigned>(hr));
    return ResultOnly(hr);
}

PayloadWriter PipeServer::HandleAuthorize(const Message& message)
{
    PayloadReader reader(message.payload);
    std::wstring sid;
    HRESULT hr = E_INVALIDARG;
    if (reader.ReadString(&sid) && reader.AtEnd() && IsValidSid(sid))
    {
        broker_.Authorize(sid);
        hr = S_OK;
    }
    LogLine(L"authorize hr=0x%08X", static_cast<unsigned>(hr));
    return ResultOnly(hr);
}

PayloadWriter PipeServer::HandleStartPairing(const Message& message)
{
    PayloadReader reader(message.payload);
    std::wstring sid;
    std::vector<std::wstring> texts;
    if (!pairing_)
    {
        return ResultOnly(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
    }
    if (!reader.ReadString(&sid) || !reader.ReadStringList(&texts) || !reader.AtEnd() || !IsValidSid(sid))
    {
        return ResultOnly(E_INVALIDARG);
    }

    std::vector<pairing_protocol::Endpoint> endpoints;
    for (const std::wstring& text : texts)
    {
        std::optional<pairing_protocol::Endpoint> endpoint = pairing_protocol::ParseEndpoint(Utf8FromWide(text));
        if (!endpoint)
        {
            return ResultOnly(E_INVALIDARG);
        }
        endpoints.push_back(std::move(*endpoint));
    }

    const std::optional<std::string> qr = pairing_->Start(sid, endpoints);
    if (!qr)
    {
        return ResultOnly(E_FAIL);
    }
    PayloadWriter reply;
    reply.WriteInt32(S_OK);
    reply.WriteString(WideFromUtf8(*qr));
    return reply;
}

PayloadWriter PipeServer::HandlePairingStatus()
{
    if (!pairing_)
    {
        return ResultOnly(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
    }
    const PairingStatus status = pairing_->Status();
    PayloadWriter reply;
    reply.WriteInt32(S_OK);
    reply.WriteInt32(static_cast<int32_t>(status.stage));
    reply.WriteString(status.code);
    reply.WriteString(status.phoneName);
    return reply;
}

PayloadWriter PipeServer::HandleConfirmPairing(const Message& message)
{
    PayloadReader reader(message.payload);
    int32_t accepted = 0;
    if (!pairing_ || !reader.ReadInt32(&accepted) || !reader.AtEnd())
    {
        return ResultOnly(E_INVALIDARG);
    }
    return ResultOnly(pairing_->ConfirmOnPc(accepted != 0) ? S_OK : E_ILLEGAL_STATE_CHANGE);
}
