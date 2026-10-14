#include "host/service_host.h"
#include "ipc/admin_requests.h"
#include "ipc/logon_ui_session.h"
#include "ipc/pipe_server.h"
#include "log.h"
#include "network/api.h"
#include "network/http_server.h"
#include "network/state_endpoint.h"
#include "network/unlock_endpoint.h"
#include "session/account_name.h"
#include "session/session_monitor.h"
#include "pipe_io.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <optional>
#include <thread>
#include <vector>

using pipe_protocol::Message;
using pipe_protocol::MessageType;
using pipe_protocol::PayloadWriter;

namespace
{
constexpr wchar_t LogDirectory[] = L"%ProgramData%\\PhoneUnlock\\logs";
constexpr wchar_t ServiceLogFile[] = L"service.log";
constexpr wchar_t ConsoleLogFile[] = L"console.log";
constexpr wchar_t SecretsDirectory[] = L"%ProgramData%\\PhoneUnlock\\secrets";
constexpr wchar_t StateDirectory[] = L"%ProgramData%\\PhoneUnlock\\state";
constexpr wchar_t PairingsDirectory[] = L"%ProgramData%\\PhoneUnlock\\state\\pairings";
constexpr wchar_t HttpsPrefix[] = L"https://+:47863/v1/";
constexpr DWORD StopWaitHint = 5000;
constexpr auto StateLongPoll = std::chrono::seconds(25);
constexpr auto ResultLongPoll = std::chrono::seconds(10);

SERVICE_STATUS_HANDLE statusHandle = nullptr;
SERVICE_STATUS status = {};
HANDLE serviceStopEvent = nullptr;
std::atomic<SessionMonitor*> sessionMonitor = nullptr;

std::wstring Expand(PCWSTR path)
{
    wchar_t expanded[MAX_PATH];
    return ExpandEnvironmentStringsW(path, expanded, MAX_PATH) ? std::wstring(expanded) : std::wstring();
}

void ReportStatus(DWORD state, DWORD exitCode = NO_ERROR)
{
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwWin32ExitCode = exitCode;
    status.dwControlsAccepted =
        state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE : 0;
    status.dwWaitHint = state == SERVICE_RUNNING ? 0 : StopWaitHint;
    SetServiceStatus(statusHandle, &status);
}

DWORD WINAPI ControlHandler(DWORD control, DWORD eventType, void* eventData, void*)
{
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN)
    {
        ReportStatus(SERVICE_STOP_PENDING);
        SetEvent(serviceStopEvent);
        return NO_ERROR;
    }
    if (control == SERVICE_CONTROL_SESSIONCHANGE)
    {
        SessionMonitor* monitor = sessionMonitor.load();
        if (monitor && eventData)
        {
            monitor->OnSessionEvent(eventType, static_cast<const WTSSESSION_NOTIFICATION*>(eventData)->dwSessionId);
        }
        return NO_ERROR;
    }
    return control == SERVICE_CONTROL_INTERROGATE ? NO_ERROR : ERROR_CALL_NOT_IMPLEMENTED;
}

void WINAPI ServiceMain(DWORD, LPWSTR*)
{
    statusHandle = RegisterServiceCtrlHandlerExW(pipe_protocol::ServiceName, ControlHandler, nullptr);
    if (!statusHandle)
    {
        return;
    }
    ReportStatus(SERVICE_START_PENDING);

    serviceStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!serviceStopEvent)
    {
        ReportStatus(SERVICE_STOPPED, GetLastError());
        return;
    }

    ReportStatus(SERVICE_RUNNING);
    RunService(serviceStopEvent);
    CloseHandle(serviceStopEvent);
    ReportStatus(SERVICE_STOPPED);
}

BOOL WINAPI ConsoleHandler(DWORD)
{
    SetEvent(serviceStopEvent);
    return TRUE;
}
}

void RunService(HANDLE stop)
{
    LogLine(L"service started");
    SecretStore secrets(Expand(SecretsDirectory));
    ChangeFeed changes;
    LogonTiles tiles(changes);
    SessionMonitor sessions(changes);
    sessionMonitor = &sessions;
    PairingStore pairings(Expand(PairingsDirectory));

    std::optional<PairingManager> pairing;
    std::string pcName;
    cbor::Bytes pcId;
    if (std::optional<PcIdentity> identity = LoadPcIdentity(Expand(StateDirectory)))
    {
        pcName = identity->name;
        pcId = identity->pcId;
        pairing.emplace(std::move(*identity), pairings);
    }

    UnlockRequests unlockRequests(tiles, pairings, changes, pcId, pcName, AccountDisplayName);
    LogonUiSession logonUiSession(secrets, tiles, unlockRequests);
    AdminRequests adminRequests(secrets, tiles, pairings, pairing ? &*pairing : nullptr);
    PipeServer logonUiServer(PipeRole::LogonUi, [&logonUiSession](HANDLE pipe, Message first, HANDLE connectionStop) {
        logonUiSession.Serve(pipe, std::move(first), connectionStop);
    });
    PipeServer adminServer(PipeRole::Administrator, [&adminRequests](HANDLE pipe, Message first, HANDLE connectionStop) {
        const PayloadWriter reply = adminRequests.Handle(first);
        pipe_protocol::WriteMessage(pipe, MessageType::Ack, reply.Bytes(), connectionStop);
    });

    std::vector<std::thread> servers;
    servers.emplace_back([&adminServer, stop] { adminServer.Run(stop); });

    StateEndpoint state(pairings, changes,
        [&tiles, &sessions](const std::wstring& sid) { return DescribeAccount(tiles.Find(sid), sessions.Summary()); },
        pcName, StateLongPoll);
    UnlockEndpoint unlock(pairings, unlockRequests, ResultLongPoll);
    std::optional<Api> api;
    std::optional<HttpServer> https;
    if (pairing)
    {
        api.emplace(*pairing, state, unlock);
        https.emplace(HttpsPrefix, [&api](const HttpRequest& request) { return api->Handle(request); });
        servers.emplace_back([&https, stop] { https->Run(stop); });
    }
    else
    {
        LogLine(L"https server disabled: pc identity unavailable");
    }

    logonUiServer.Run(stop);
    changes.Close();
    unlockRequests.Close();
    for (std::thread& server : servers)
    {
        server.join();
    }
    sessionMonitor = nullptr;
    LogLine(L"service stopped");
}

int RunAsService()
{
    ConfigureLog(LogDirectory, ServiceLogFile);
    SERVICE_TABLE_ENTRYW table[] = {
        { const_cast<LPWSTR>(pipe_protocol::ServiceName), ServiceMain },
        { nullptr, nullptr },
    };
    return StartServiceCtrlDispatcherW(table) ? 0 : static_cast<int>(GetLastError());
}

int RunInConsole()
{
    ConfigureLog(LogDirectory, ConsoleLogFile);
    serviceStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!serviceStopEvent)
    {
        return static_cast<int>(GetLastError());
    }
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    fwprintf(stdout, L"Service en mode console. Ctrl+C pour arrêter.\n");
    RunService(serviceStopEvent);
    CloseHandle(serviceStopEvent);
    return 0;
}
