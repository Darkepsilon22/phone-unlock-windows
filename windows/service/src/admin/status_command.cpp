#include <windows.h>
#include <winhttp.h>
#include "admin/status_command.h"
#include "admin/console.h"
#include "admin/current_user.h"
#include "admin/local_endpoints.h"
#include "admin/service_client.h"
#include "pairing/pairing_protocol.h"
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

using pipe_protocol::Message;
using pipe_protocol::MessageType;
using pipe_protocol::PayloadReader;
using pipe_protocol::PayloadWriter;

namespace
{
constexpr wchar_t ProviderKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Authentication\\Credential Providers\\"
    L"{512D55BA-432F-42D6-95EF-8071913597AA}";
constexpr wchar_t ProviderLibrary[] = L"\\PhoneUnlockProtoCP.dll";
constexpr wchar_t VersionQuery[] = L"\\StringFileInfo\\040C04B0\\ProductVersion";
constexpr wchar_t ProbeHost[] = L"127.0.0.1";
constexpr wchar_t ProbePath[] = L"/v1/";
constexpr int ProbeTimeout = 3000;
constexpr DWORD NotFound = 404;
constexpr DWORD ServiceConfigSize = 8 * 1024;
constexpr DWORD IgnoredCertificateErrors = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
    SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;

struct ServiceInfo
{
    DWORD state = 0;
    DWORD startType = 0;
    std::wstring binary;
};

struct ServiceView
{
    bool passwordSaved = false;
    std::vector<std::wstring> phones;
    std::wstring unlockTime;
    std::wstring unlockPhone;
    int32_t unlockStatus = 0;
    std::wstring errorTime;
    std::wstring errorPhone;
    std::wstring errorCode;
};

struct ProbeResult
{
    std::optional<DWORD> status;
    DWORD error = ERROR_SUCCESS;
};

std::wstring Unquoted(std::wstring path)
{
    if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"')
    {
        return path.substr(1, path.size() - 2);
    }
    return path;
}

std::optional<ServiceInfo> QueryService()
{
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager)
    {
        return std::nullopt;
    }
    std::optional<ServiceInfo> info;
    SC_HANDLE service = OpenServiceW(manager, pipe_protocol::ServiceName, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (service)
    {
        SERVICE_STATUS_PROCESS status = {};
        std::vector<BYTE> buffer(ServiceConfigSize);
        auto* config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());
        DWORD needed = 0;
        if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<BYTE*>(&status), sizeof(status),
                &needed) &&
            QueryServiceConfigW(service, config, ServiceConfigSize, &needed))
        {
            info = ServiceInfo{ status.dwCurrentState, config->dwStartType, Unquoted(config->lpBinaryPathName) };
        }
        CloseServiceHandle(service);
    }
    CloseServiceHandle(manager);
    return info;
}

const wchar_t* StateText(DWORD state)
{
    switch (state)
    {
    case SERVICE_RUNNING:
        return L"en cours";
    case SERVICE_STOPPED:
        return L"arrêté";
    default:
        return L"en cours de démarrage ou d'arrêt";
    }
}

const wchar_t* StartText(DWORD startType)
{
    switch (startType)
    {
    case SERVICE_AUTO_START:
        return L"démarrage automatique";
    case SERVICE_DEMAND_START:
        return L"démarrage manuel";
    default:
        return L"désactivé";
    }
}

std::wstring FileVersion(const std::wstring& path)
{
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0)
    {
        return L"absente";
    }
    std::vector<BYTE> data(size);
    wchar_t* version = nullptr;
    UINT length = 0;
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()) ||
        !VerQueryValueW(data.data(), VersionQuery, reinterpret_cast<void**>(&version), &length) || length == 0)
    {
        return L"sans numéro";
    }
    return version;
}

std::wstring ProviderPath()
{
    wchar_t directory[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(directory, MAX_PATH);
    return length > 0 && length < MAX_PATH ? std::wstring(directory) + ProviderLibrary : std::wstring();
}

bool ProviderRegistered()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, ProviderKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
    {
        return false;
    }
    RegCloseKey(key);
    return true;
}

ProbeResult SendProbe(HINTERNET session)
{
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols)) ||
        !WinHttpSetTimeouts(session, ProbeTimeout, ProbeTimeout, ProbeTimeout, ProbeTimeout))
    {
        return { std::nullopt, GetLastError() };
    }
    HINTERNET connection = WinHttpConnect(session, ProbeHost, pairing_protocol::DefaultPort, 0);
    if (!connection)
    {
        return { std::nullopt, GetLastError() };
    }
    ProbeResult result;
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", ProbePath, nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    DWORD flags = IgnoredCertificateErrors;
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (request && WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags)) &&
        WinHttpSetOption(request, WINHTTP_OPTION_CLIENT_CERT_CONTEXT, WINHTTP_NO_CLIENT_CERT_CONTEXT, 0) &&
        WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr) &&
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
            &status, &statusSize, WINHTTP_NO_HEADER_INDEX))
    {
        result.status = status;
    }
    else
    {
        result.error = GetLastError();
    }
    if (request)
    {
        WinHttpCloseHandle(request);
    }
    WinHttpCloseHandle(connection);
    return result;
}

ProbeResult ProbeHttps()
{
    HINTERNET session = WinHttpOpen(L"PhoneUnlock status", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
    {
        return { std::nullopt, GetLastError() };
    }
    const ProbeResult result = SendProbe(session);
    WinHttpCloseHandle(session);
    return result;
}

std::optional<ServiceView> QueryServiceView(const std::wstring& sid)
{
    PayloadWriter request;
    request.WriteString(sid);
    const std::optional<Message> reply = ExchangeWithService(MessageType::Status, request);
    if (!reply)
    {
        return std::nullopt;
    }
    PayloadReader reader(reply->payload);
    ServiceView view;
    int32_t passwordSaved = 0;
    if (!ReadSuccess(reader) || !reader.ReadInt32(&passwordSaved) || !reader.ReadStringList(&view.phones) ||
        !reader.ReadString(&view.unlockTime) || !reader.ReadString(&view.unlockPhone) ||
        !reader.ReadInt32(&view.unlockStatus) || !reader.ReadString(&view.errorTime) ||
        !reader.ReadString(&view.errorPhone) || !reader.ReadString(&view.errorCode) || !reader.AtEnd())
    {
        return std::nullopt;
    }
    view.passwordSaved = passwordSaved != 0;
    return view;
}

std::wstring Joined(const std::vector<std::wstring>& texts)
{
    std::wstring joined;
    for (const std::wstring& text : texts)
    {
        joined += (joined.empty() ? L"" : L", ") + text;
    }
    return joined;
}

const wchar_t* ErrorText(const std::wstring& code)
{
    if (code == L"REVOKED")
    {
        return L"téléphone inconnu ou révoqué";
    }
    if (code == L"INVALID_PROOF")
    {
        return L"signature refusée : clé du téléphone invalide";
    }
    if (code == L"WINDOWS_REJECTED")
    {
        return L"mot de passe refusé par Windows : relance provision";
    }
    if (code == L"EXPIRED")
    {
        return L"demande expirée";
    }
    if (code == L"WRONG_CONTEXT")
    {
        return L"écran de verrouillage changé pendant la demande";
    }
    return L"erreur du protocole";
}

void PrintLocalChecks(const ServiceInfo& service)
{
    const std::wstring provider = ProviderPath();
    fwprintf(stdout, L"Versions : service %s, tuile %s (%s)\n", FileVersion(service.binary).c_str(),
        FileVersion(provider).c_str(), ProviderRegistered() ? L"enregistrée" : L"non enregistrée");

    const std::vector<std::wstring> endpoints = LocalIpv4Endpoints(pairing_protocol::DefaultPort);
    fwprintf(stdout, L"Adresses du PC : %s\n",
        endpoints.empty() ? L"aucune, pas de réseau local" : Joined(endpoints).c_str());

    const ProbeResult probe = ProbeHttps();
    if (!probe.status)
    {
        fwprintf(stdout, L"Serveur HTTPS : ne répond pas sur le port %u (erreur %lu)\n", pairing_protocol::DefaultPort,
            probe.error);
    }
    else if (*probe.status == NotFound)
    {
        fwprintf(stdout, L"Serveur HTTPS : répond en TLS 1.3 sur le port %u\n", pairing_protocol::DefaultPort);
    }
    else
    {
        fwprintf(stdout, L"Serveur HTTPS : réponse inattendue %lu\n", *probe.status);
    }
}

void PrintServiceView(const ServiceView& view)
{
    fwprintf(stdout, L"Mot de passe enregistré pour ce compte : %s\n",
        view.passwordSaved ? L"oui" : L"non, lance provision");
    if (view.phones.empty())
    {
        fwprintf(stdout, L"Téléphones associés à ce compte : aucun\n");
    }
    else
    {
        fwprintf(stdout, L"Téléphones associés à ce compte : %zu (%s)\n", view.phones.size(), Joined(view.phones).c_str());
    }

    if (view.unlockTime.empty())
    {
        fwprintf(stdout, L"Dernier déverrouillage : aucun depuis le démarrage du service\n");
    }
    else if (view.unlockStatus == 0)
    {
        fwprintf(stdout, L"Dernier déverrouillage : %s, %s, réussi\n", LocalDateTime(view.unlockTime).c_str(),
            view.unlockPhone.c_str());
    }
    else
    {
        fwprintf(stdout, L"Dernier déverrouillage : %s, %s, refusé par Windows (0x%08X)\n",
            LocalDateTime(view.unlockTime).c_str(), view.unlockPhone.c_str(), static_cast<unsigned>(view.unlockStatus));
    }

    if (view.errorTime.empty())
    {
        fwprintf(stdout, L"Dernière erreur : aucune depuis le démarrage du service\n");
        return;
    }
    fwprintf(stdout, L"Dernière erreur : %s, %s (%s)%s%s\n", LocalDateTime(view.errorTime).c_str(),
        ErrorText(view.errorCode), view.errorCode.c_str(), view.errorPhone.empty() ? L"" : L", ",
        view.errorPhone.c_str());
}
}

int RunStatusCommand()
{
    const std::optional<ServiceInfo> service = QueryService();
    if (!service)
    {
        fwprintf(stdout, L"Service : non installé\n");
        return exit_code::Failure;
    }
    fwprintf(stdout, L"Service : %s, %s\n", StateText(service->state), StartText(service->startType));
    PrintLocalChecks(*service);

    const std::optional<std::wstring> sid = CurrentUserSid();
    const std::optional<ServiceView> view = sid ? QueryServiceView(*sid) : std::nullopt;
    if (!view)
    {
        return exit_code::Failure;
    }
    PrintServiceView(*view);
    return service->state == SERVICE_RUNNING ? exit_code::Success : exit_code::Failure;
}
