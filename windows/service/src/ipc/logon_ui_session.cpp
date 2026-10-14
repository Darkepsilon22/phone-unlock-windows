#include "ipc/logon_ui_session.h"
#include "encoding/text.h"
#include "log.h"
#include <algorithm>
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

std::optional<ContextId> ReadContext(PayloadReader& reader)
{
    std::vector<uint8_t> bytes;
    if (!reader.ReadBytes(&bytes) || bytes.size() != std::tuple_size_v<ContextId>)
    {
        return std::nullopt;
    }
    ContextId context;
    std::copy(bytes.begin(), bytes.end(), context.begin());
    return context;
}

bool IsSignaled(HANDLE event)
{
    return WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
}

struct LogonResult
{
    std::wstring sid;
    int32_t status = 0;
};

std::optional<LogonResult> ReadResult(const Message& message)
{
    PayloadReader reader(message.payload);
    LogonResult result;
    int32_t subStatus = 0;
    if (!reader.ReadString(&result.sid) || !reader.ReadInt32(&result.status) || !reader.ReadInt32(&subStatus))
    {
        return std::nullopt;
    }
    LogLine(L"logon result status=0x%08X substatus=0x%08X", static_cast<unsigned>(result.status),
        static_cast<unsigned>(subStatus));
    return result;
}
}

LogonUiSession::LogonUiSession(const SecretStore& secrets, LogonTiles& tiles, UnlockRequests& requests)
    : secrets_(secrets), tiles_(tiles), requests_(requests)
{
}

void LogonUiSession::Serve(HANDLE pipe, Message message, HANDLE stopEvent)
{
    LogLine(L"logonui connected");
    for (;;)
    {
        if (message.type == MessageType::Result)
        {
            if (const std::optional<LogonResult> result = ReadResult(message))
            {
                requests_.ReportLogonResult(result->sid, result->status);
            }
        }
        else if (message.type == MessageType::Wait)
        {
            if (!WaitAndSendReady(pipe, message, stopEvent))
            {
                return;
            }
        }
        else if (message.type == MessageType::RequestUnlock)
        {
            AnswerTileRequest(pipe, message, stopEvent);
            return;
        }
        else if (message.type == MessageType::CancelUnlock)
        {
            CancelTileRequest(pipe, message, stopEvent);
            return;
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

bool LogonUiSession::WaitAndSendReady(HANDLE pipe, const Message& wait, HANDLE stopEvent)
{
    std::vector<std::wstring> sids;
    int32_t scenario = 0;
    std::optional<ContextId> context;
    PayloadReader reader(wait.payload);
    if (!reader.ReadStringList(&sids) || !reader.ReadInt32(&scenario) || !(context = ReadContext(reader)) ||
        !reader.AtEnd() || !IsKnownScenario(scenario))
    {
        return false;
    }
    const uint64_t ticket = tiles_.Register(static_cast<TileScenario>(scenario), std::move(sids), *context);
    const auto keepWaiting = [pipe, stopEvent] { return !IsSignaled(stopEvent) && IsConnected(pipe); };
    bool sent = false;
    for (;;)
    {
        const std::optional<std::wstring> sid = tiles_.WaitForAuthorization(ticket, keepWaiting);
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
    tiles_.Unregister(ticket);
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

void LogonUiSession::AnswerTileRequest(HANDLE pipe, const Message& request, HANDLE stopEvent)
{
    std::wstring sid;
    std::optional<ContextId> context;
    PayloadReader reader(request.payload);
    TileRequest outcome;
    if (reader.ReadString(&sid) && (context = ReadContext(reader)) && reader.AtEnd())
    {
        outcome = requests_.CreateFromTile(sid, *context);
    }
    LogLine(L"tile request outcome=%d", static_cast<int>(outcome.outcome));

    PayloadWriter reply;
    if (reply.WriteInt32(static_cast<int32_t>(outcome.outcome)) && reply.WriteString(WideFromUtf8(outcome.matchCode)))
    {
        pipe_protocol::WriteMessage(pipe, MessageType::UnlockCode, reply.Bytes(), stopEvent);
    }
}

void LogonUiSession::CancelTileRequest(HANDLE pipe, const Message& request, HANDLE stopEvent)
{
    std::wstring sid;
    std::optional<ContextId> context;
    PayloadReader reader(request.payload);
    if (reader.ReadString(&sid) && (context = ReadContext(reader)) && reader.AtEnd())
    {
        requests_.CancelFromTile(sid, *context);
    }
    pipe_protocol::WriteMessage(pipe, MessageType::Ack, {}, stopEvent);
}
