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

bool IsKnownScenario(int32_t scenario)
{
    return scenario == static_cast<int32_t>(TileScenario::Logon) || scenario == static_cast<int32_t>(TileScenario::Unlock);
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

LogonUiSession::LogonUiSession(const SecretStore& secrets, LogonTiles& tiles) : secrets_(secrets), tiles_(tiles)
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
            int32_t scenario = 0;
            PayloadReader reader(message.payload);
            if (!reader.ReadStringList(&sids) || !reader.ReadInt32(&scenario) || !reader.AtEnd() ||
                !IsKnownScenario(scenario) ||
                !WaitAndSendReady(pipe, static_cast<TileScenario>(scenario), std::move(sids), stopEvent))
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

bool LogonUiSession::WaitAndSendReady(HANDLE pipe, TileScenario scenario, std::vector<std::wstring> sids,
    HANDLE stopEvent)
{
    const std::optional<uint64_t> ticket = tiles_.Register(scenario, std::move(sids));
    if (!ticket)
    {
        return false;
    }
    const auto keepWaiting = [pipe, stopEvent] { return !IsSignaled(stopEvent) && IsConnected(pipe); };
    bool sent = false;
    for (;;)
    {
        const std::optional<std::wstring> sid = tiles_.WaitForAuthorization(*ticket, keepWaiting);
        if (!sid)
        {
            break;
        }
        SecureString password;
        const HRESULT hr = secrets_.Load(*sid, &password);
        if (SUCCEEDED(hr))
        {
            sent = SendReady(pipe, *sid, password, stopEvent);
            break;
        }
        LogLine(L"secret unavailable hr=0x%08X", static_cast<unsigned>(hr));
    }
    tiles_.Unregister(*ticket);
    return sent;
}

bool LogonUiSession::SendReady(HANDLE pipe, const std::wstring& sid, const SecureString& password, HANDLE stopEvent)
{
    PayloadWriter writer;
    const bool sent = writer.WriteString(sid) && writer.WriteString(password.c_str(), password.length()) &&
        pipe_protocol::WriteMessage(pipe, MessageType::Ready, writer.Bytes(), stopEvent);
    LogLine(L"ready sent=%d", sent);
    return sent;
}
