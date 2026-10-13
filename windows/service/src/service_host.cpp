#include "service_host.h"
#include "log.h"
#include "pipe_protocol.h"
#include "pipe_server.h"
#include <cstdio>
#include <thread>

namespace
{
constexpr wchar_t LogDirectory[] = L"%ProgramData%\\PhoneUnlock\\logs";
constexpr wchar_t ServiceLogFile[] = L"service.log";
constexpr wchar_t ConsoleLogFile[] = L"console.log";
constexpr wchar_t SecretsDirectory[] = L"%ProgramData%\\PhoneUnlock\\secrets";
constexpr DWORD StopWaitHint = 5000;

SERVICE_STATUS_HANDLE statusHandle = nullptr;
SERVICE_STATUS status = {};
HANDLE stopEvent = nullptr;

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
    status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    status.dwWaitHint = state == SERVICE_RUNNING ? 0 : StopWaitHint;
    SetServiceStatus(statusHandle, &status);
}

DWORD WINAPI ControlHandler(DWORD control, DWORD, void*, void*)
{
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN)
    {
        ReportStatus(SERVICE_STOP_PENDING);
        SetEvent(stopEvent);
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

    stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent)
    {
        ReportStatus(SERVICE_STOPPED, GetLastError());
        return;
    }

    ReportStatus(SERVICE_RUNNING);
    RunService(stopEvent);
    CloseHandle(stopEvent);
    ReportStatus(SERVICE_STOPPED);
}

BOOL WINAPI ConsoleHandler(DWORD)
{
    SetEvent(stopEvent);
    return TRUE;
}
}

void RunService(HANDLE stop)
{
    LogLine(L"service started");
    SecretStore secrets(Expand(SecretsDirectory));
    AuthorizationBroker broker;
    PipeServer logonUiServer(PipeRole::LogonUi, secrets, broker);
    PipeServer adminServer(PipeRole::Administrator, secrets, broker);
    std::thread adminThread([&adminServer, stop] { adminServer.Run(stop); });
    logonUiServer.Run(stop);
    adminThread.join();
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
    stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent)
    {
        return static_cast<int>(GetLastError());
    }
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    fwprintf(stdout, L"Service en mode console. Ctrl+C pour arrêter.\n");
    RunService(stopEvent);
    CloseHandle(stopEvent);
    return 0;
}
