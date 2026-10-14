#include "service_listener.h"
#include "log.h"
#include "pipe_io.h"

using pipe_protocol::MessageType;
using pipe_protocol::PayloadReader;
using pipe_protocol::PayloadWriter;

namespace
{
constexpr DWORD ReconnectDelay = 2000;
constexpr DWORD ConnectTimeout = 1000;
constexpr DWORD ResultTimeout = 60'000;

DWORD ServiceProcessId()
{
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager)
    {
        return 0;
    }
    SC_HANDLE service = OpenServiceW(manager, pipe_protocol::ServiceName, SERVICE_QUERY_STATUS);
    SERVICE_STATUS_PROCESS status = {};
    DWORD needed = 0;
    const bool queried = service &&
        QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<BYTE*>(&status), sizeof(status), &needed);
    if (service)
    {
        CloseServiceHandle(service);
    }
    CloseServiceHandle(manager);
    return queried && status.dwCurrentState == SERVICE_RUNNING ? status.dwProcessId : 0;
}
}

ServiceListener::ServiceListener(pipe_protocol::TileScenario scenario, std::vector<std::wstring> sids,
    ReadyHandler onReady)
    : scenario_(scenario),
      sids_(std::move(sids)),
      onReady_(std::move(onReady)),
      stopEvent_(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
      resultEvent_(CreateEventW(nullptr, FALSE, FALSE, nullptr))
{
    if (stopEvent_ && resultEvent_)
    {
        worker_ = std::thread(&ServiceListener::Run, this);
    }
}

ServiceListener::~ServiceListener()
{
    if (stopEvent_)
    {
        SetEvent(stopEvent_);
    }
    if (worker_.joinable())
    {
        worker_.join();
    }
    if (stopEvent_)
    {
        CloseHandle(stopEvent_);
    }
    if (resultEvent_)
    {
        CloseHandle(resultEvent_);
    }
}

void ServiceListener::ReportResult(const std::wstring& sid, LONG status, LONG subStatus)
{
    {
        std::lock_guard lock(mutex_);
        result_ = PendingResult{ sid, status, subStatus };
    }
    SetEvent(resultEvent_);
}

const std::vector<std::wstring>& ServiceListener::Sids() const
{
    return sids_;
}

void ServiceListener::Run()
{
    LogLine(L"listener started users=%zu", sids_.size());
    while (WaitForSingleObject(stopEvent_, 0) != WAIT_OBJECT_0)
    {
        HANDLE pipe = Connect();
        if (pipe != INVALID_HANDLE_VALUE)
        {
            Converse(pipe);
            CloseHandle(pipe);
        }
        WaitForSingleObject(stopEvent_, ReconnectDelay);
    }
    LogLine(L"listener stopped");
}

HANDLE ServiceListener::Connect()
{
    HANDLE pipe = pipe_protocol::ConnectToService(pipe_protocol::LogonUiPipeName, stopEvent_, ConnectTimeout);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        return pipe;
    }
    if (!IsGenuineService(pipe))
    {
        LogLine(L"pipe server rejected");
        CloseHandle(pipe);
        return INVALID_HANDLE_VALUE;
    }
    return pipe;
}

bool ServiceListener::IsGenuineService(HANDLE pipe) const
{
    ULONG serverProcessId = 0;
    return GetNamedPipeServerProcessId(pipe, &serverProcessId) && serverProcessId != 0 &&
        serverProcessId == ServiceProcessId();
}

void ServiceListener::Converse(HANDLE pipe)
{
    for (;;)
    {
        PayloadWriter wait;
        if (!wait.WriteStringList(sids_) || !wait.WriteInt32(static_cast<int32_t>(scenario_)) ||
            !pipe_protocol::WriteMessage(pipe, MessageType::Wait, wait.Bytes(), stopEvent_))
        {
            return;
        }

        std::optional<pipe_protocol::Message> message = pipe_protocol::ReadMessage(pipe, stopEvent_);
        if (!message || message->type != MessageType::Ready)
        {
            return;
        }

        PayloadReader reader(message->payload);
        std::wstring sid;
        SecureString password;
        if (!reader.ReadString(&sid) || !reader.ReadSecret(&password) || !reader.AtEnd())
        {
            return;
        }
        LogLine(L"authorization received");
        onReady_(sid, std::move(password));

        if (!SendResultWhenReady(pipe))
        {
            return;
        }
    }
}

bool ServiceListener::SendResultWhenReady(HANDLE pipe)
{
    HANDLE handles[] = { resultEvent_, stopEvent_ };
    if (WaitForMultipleObjects(2, handles, FALSE, ResultTimeout) != WAIT_OBJECT_0)
    {
        return WaitForSingleObject(stopEvent_, 0) != WAIT_OBJECT_0;
    }

    std::optional<PendingResult> result;
    {
        std::lock_guard lock(mutex_);
        result.swap(result_);
    }
    if (!result)
    {
        return true;
    }

    PayloadWriter writer;
    return writer.WriteString(result->sid) && writer.WriteInt32(result->status) &&
        writer.WriteInt32(result->subStatus) &&
        pipe_protocol::WriteMessage(pipe, MessageType::Result, writer.Bytes(), stopEvent_);
}
