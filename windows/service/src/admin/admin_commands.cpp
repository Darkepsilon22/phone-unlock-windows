#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "admin/admin_commands.h"
#include "pairing/pairing_manager.h"
#include "pipe_io.h"
#include "admin/qr_console.h"
#include "storage/secret_store.h"
#include "host/service_host.h"
#include "encoding/text.h"
#include <sddl.h>
#include <cstdio>
#include <cwchar>
#include <optional>
#include <string>

using pipe_protocol::Message;
using pipe_protocol::MessageType;
using pipe_protocol::PayloadReader;
using pipe_protocol::PayloadWriter;

namespace
{
constexpr DWORD ConnectTimeout = 3000;
constexpr DWORD MaxPasswordLength = 256;
constexpr DWORD MillisecondsPerSecond = 1000;
constexpr DWORD StatusPollInterval = 1000;
constexpr DWORD PairingTimeout = 130'000;
constexpr ULONG AdapterBufferSize = 32 * 1024;
constexpr int Success = 0;
constexpr int Failure = 1;

std::optional<std::wstring> CurrentUserSid()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        return std::nullopt;
    }
    BYTE buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    DWORD size = 0;
    const BOOL read = GetTokenInformation(token, TokenUser, buffer, sizeof(buffer), &size);
    CloseHandle(token);

    LPWSTR text = nullptr;
    if (!read || !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer)->User.Sid, &text))
    {
        return std::nullopt;
    }
    std::wstring sid(text);
    LocalFree(text);
    return sid;
}

std::optional<SecureString> ReadHiddenLine()
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (!GetConsoleMode(input, &mode) || !SetConsoleMode(input, mode & ~ENABLE_ECHO_INPUT))
    {
        return std::nullopt;
    }

    wchar_t buffer[MaxPasswordLength + 2] = {};
    DWORD read = 0;
    const BOOL ok = ReadConsoleW(input, buffer, MaxPasswordLength + 2, &read, nullptr);
    SetConsoleMode(input, mode);
    fwprintf(stdout, L"\n");

    while (read > 0 && (buffer[read - 1] == L'\r' || buffer[read - 1] == L'\n'))
    {
        --read;
    }
    std::optional<SecureString> line;
    if (ok && read > 0 && read <= MaxPasswordLength)
    {
        line = SecureString(buffer, read);
    }
    SecureZeroMemory(buffer, sizeof(buffer));
    return line;
}

bool AskYesNo(const wchar_t* question)
{
    fwprintf(stdout, L"%s (o/n) : ", question);
    wchar_t answer[8] = {};
    DWORD read = 0;
    ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), answer, 8, &read, nullptr);
    return read > 0 && (answer[0] == L'o' || answer[0] == L'O');
}

std::vector<std::wstring> LocalEndpoints()
{
    std::vector<std::wstring> endpoints;
    std::vector<BYTE> buffer(AdapterBufferSize);
    ULONG size = AdapterBufferSize;
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size) != NO_ERROR)
    {
        return endpoints;
    }
    for (auto* adapter = adapters; adapter; adapter = adapter->Next)
    {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
        {
            continue;
        }
        for (auto* address = adapter->FirstUnicastAddress; address; address = address->Next)
        {
            wchar_t text[INET_ADDRSTRLEN] = {};
            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(address->Address.lpSockaddr);
            if (InetNtopW(AF_INET, &ipv4->sin_addr, text, INET_ADDRSTRLEN))
            {
                endpoints.push_back(std::wstring(text) + L":" + std::to_wstring(pairing_protocol::DefaultPort));
            }
        }
    }
    return endpoints;
}

std::optional<Message> Exchange(MessageType type, const PayloadWriter& writer)
{
    HANDLE pipe = pipe_protocol::ConnectToService(pipe_protocol::AdminPipeName, nullptr, ConnectTimeout);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        const DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED)
        {
            fwprintf(stderr, L"Accès refusé : lance la commande dans un terminal administrateur.\n");
        }
        else
        {
            fwprintf(stderr, L"Service injoignable (erreur %lu). Est-il démarré ?\n", error);
        }
        return std::nullopt;
    }

    std::optional<Message> reply;
    if (pipe_protocol::WriteMessage(pipe, type, writer.Bytes(), nullptr))
    {
        reply = pipe_protocol::ReadMessage(pipe, nullptr);
    }
    CloseHandle(pipe);
    if (!reply || reply->type != MessageType::Ack)
    {
        fwprintf(stderr, L"Réponse du service invalide.\n");
        return std::nullopt;
    }
    return reply;
}

bool Succeeded(PayloadReader& reader)
{
    int32_t hr = E_FAIL;
    if (!reader.ReadInt32(&hr))
    {
        fwprintf(stderr, L"Réponse du service invalide.\n");
        return false;
    }
    if (FAILED(hr))
    {
        fwprintf(stderr, L"Refusé par le service : 0x%08X\n", static_cast<unsigned>(hr));
        return false;
    }
    return true;
}

int SendSimple(MessageType type, const PayloadWriter& writer)
{
    const std::optional<Message> reply = Exchange(type, writer);
    if (!reply)
    {
        return Failure;
    }
    PayloadReader reader(reply->payload);
    if (!Succeeded(reader))
    {
        return Failure;
    }
    fwprintf(stdout, L"OK\n");
    return Success;
}

std::optional<std::wstring> RequireUserSid()
{
    std::optional<std::wstring> sid = CurrentUserSid();
    if (!sid)
    {
        fwprintf(stderr, L"SID du compte introuvable.\n");
    }
    return sid;
}

int Provision()
{
    const std::optional<std::wstring> sid = RequireUserSid();
    if (!sid)
    {
        return Failure;
    }
    fwprintf(stdout, L"Compte %s\nMot de passe du compte (il ne s'affiche pas) : ", sid->c_str());
    const std::optional<SecureString> password = ReadHiddenLine();
    if (!password)
    {
        fwprintf(stderr, L"Mot de passe vide ou trop long.\n");
        return Failure;
    }

    PayloadWriter writer;
    writer.WriteString(*sid);
    writer.WriteString(password->c_str(), password->length());
    return SendSimple(MessageType::Provision, writer);
}

int Authorize(int argc, wchar_t** argv)
{
    const std::optional<std::wstring> sid = RequireUserSid();
    if (!sid)
    {
        return Failure;
    }

    const DWORD delay = argc >= 2 && wcscmp(argv[0], L"--delay") == 0 ? wcstoul(argv[1], nullptr, 10) : 0;
    if (delay > 0)
    {
        fwprintf(stdout, L"Autorisation dans %lu s : verrouille la session maintenant (Windows + L).\n", delay);
        Sleep(delay * MillisecondsPerSecond);
    }

    PayloadWriter writer;
    writer.WriteString(*sid);
    return SendSimple(MessageType::Authorize, writer);
}

std::vector<std::wstring> RequestedEndpoints(int argc, wchar_t** argv)
{
    std::vector<std::wstring> endpoints;
    for (int index = 0; index + 1 < argc; ++index)
    {
        if (wcscmp(argv[index], L"--endpoint") == 0)
        {
            endpoints.push_back(argv[++index]);
        }
    }
    return endpoints.empty() ? LocalEndpoints() : endpoints;
}

std::optional<PairingStatus> QueryPairingStatus()
{
    const std::optional<Message> reply = Exchange(MessageType::PairingStatus, PayloadWriter());
    if (!reply)
    {
        return std::nullopt;
    }
    PayloadReader reader(reply->payload);
    int32_t stage = 0;
    PairingStatus status;
    if (!Succeeded(reader) || !reader.ReadInt32(&stage) || !reader.ReadString(&status.code) ||
        !reader.ReadString(&status.phoneName))
    {
        return std::nullopt;
    }
    status.stage = static_cast<PairingStage>(stage);
    return status;
}

bool ConfirmOnPc(bool accepted)
{
    PayloadWriter writer;
    writer.WriteInt32(accepted ? 1 : 0);
    return SendSimple(MessageType::ConfirmPairing, writer) == Success;
}

int FollowPairing()
{
    bool asked = false;
    bool announcedPhoneStep = false;
    const ULONGLONG deadline = GetTickCount64() + PairingTimeout;
    while (GetTickCount64() < deadline)
    {
        Sleep(StatusPollInterval);
        const std::optional<PairingStatus> status = QueryPairingStatus();
        if (!status)
        {
            return Failure;
        }
        switch (status->stage)
        {
        case PairingStage::AwaitingConfirmation:
            if (!asked)
            {
                asked = true;
                fwprintf(stdout, L"\nTéléphone : %s\nCode affiché sur le PC : %s\n", status->phoneName.c_str(),
                    status->code.c_str());
                ConfirmOnPc(AskYesNo(L"Le téléphone affiche-t-il exactement le même code ?"));
            }
            break;
        case PairingStage::PcConfirmed:
            if (!announcedPhoneStep)
            {
                announcedPhoneStep = true;
                fwprintf(stdout, L"Confirme maintenant sur le téléphone avec ton empreinte.\n");
            }
            break;
        case PairingStage::Completed:
            fwprintf(stdout, L"Association enregistrée.\n");
            return Success;
        case PairingStage::Failed:
            fwprintf(stderr, L"Association refusée ou expirée.\n");
            return Failure;
        default:
            break;
        }
    }
    fwprintf(stderr, L"Délai dépassé.\n");
    return Failure;
}

int Pair(int argc, wchar_t** argv)
{
    const std::optional<std::wstring> sid = RequireUserSid();
    const std::vector<std::wstring> endpoints = RequestedEndpoints(argc, argv);
    if (!sid || endpoints.empty())
    {
        fwprintf(stderr, L"Aucune adresse réseau utilisable : précise --endpoint adresse:port.\n");
        return Failure;
    }

    PayloadWriter writer;
    writer.WriteString(*sid);
    writer.WriteStringList(endpoints);
    const std::optional<Message> reply = Exchange(MessageType::StartPairing, writer);
    if (!reply)
    {
        return Failure;
    }
    PayloadReader reader(reply->payload);
    std::wstring qr;
    if (!Succeeded(reader) || !reader.ReadString(&qr))
    {
        return Failure;
    }

    fwprintf(stdout, L"Scanne ce code avec l'application PhoneUnlock (valable 120 s) :\n\n");
    PrintQrCode(Utf8FromWide(qr));
    fwprintf(stdout, L"\nAdresses annoncées :");
    for (const std::wstring& endpoint : endpoints)
    {
        fwprintf(stdout, L" %s", endpoint.c_str());
    }
    fwprintf(stdout, L"\nEn attente du téléphone…\n");
    return FollowPairing();
}

int Usage()
{
    fwprintf(stdout,
        L"Utilisation :\n"
        L"  PhoneUnlockService console                      exécute le service au premier plan\n"
        L"  PhoneUnlockService provision                    enregistre le mot de passe du compte courant\n"
        L"  PhoneUnlockService pair [--endpoint hôte:port]  associe un téléphone au compte courant\n"
        L"  PhoneUnlockService authorize [--delay N]        simule l'autorisation du téléphone\n");
    return Failure;
}
}

int RunAdminCommand(int argc, wchar_t** argv)
{
    const std::wstring command = argv[0];
    if (command == L"console")
    {
        return RunInConsole();
    }
    if (command == L"provision")
    {
        return Provision();
    }
    if (command == L"pair")
    {
        return Pair(argc - 1, argv + 1);
    }
    if (command == L"authorize")
    {
        return Authorize(argc - 1, argv + 1);
    }
    return Usage();
}
