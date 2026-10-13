#include "ipc/logon_ui_session.h"
#include "log.h"
#include "pipe_io.h"

using pipe_protocol::Message;
using pipe_protocol::MessageType;
using pipe_protocol::PayloadReader;
using pipe_protocol::PayloadWriter;

namespace
{
bool IsConnected(HANDLE pipe)
{
    DWORD available = 0;
    return PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) != FALSE;
}

bool IsSignaled(HANDLE event)
{
    return WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
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

LogonUiSession::LogonUiSession(const SecretStore& secrets, AuthorizationBroker& broker)
    : secrets_(secrets), broker_(broker)
{
}

void LogonUiSession::Serve(HANDLE pipe, Message message, HANDLE stopEvent)
{
    LogLine(L"logonui connected");
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

bool LogonUiSession::WaitAndSendReady(HANDLE pipe, const std::vector<std::wstring>& sids, HANDLE stopEvent)
{
    const auto keepWaiting = [pipe, stopEvent] { return !IsSignaled(stopEvent) && IsConnected(pipe); };
    for (;;)
    {
        const std::optional<std::wstring> sid = broker_.WaitForAuthorization(sids, keepWaiting);
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

bool LogonUiSession::SendReady(HANDLE pipe, const std::wstring& sid, const SecureString& password, HANDLE stopEvent)
{
    PayloadWriter writer;
    const bool sent = writer.WriteString(sid) && writer.WriteString(password.c_str(), password.length()) &&
        pipe_protocol::WriteMessage(pipe, MessageType::Ready, writer.Bytes(), stopEvent);
    LogLine(L"ready sent=%d", sent);
    return sent;
}
