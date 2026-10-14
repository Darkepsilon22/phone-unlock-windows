#include "admin/pair_command.h"
#include "admin/console.h"
#include "admin/current_user.h"
#include "admin/local_endpoints.h"
#include "admin/qr_console.h"
#include "admin/service_client.h"
#include "encoding/text.h"
#include "pairing/pairing_manager.h"
#include <cstdio>
#include <cwchar>

using pipe_protocol::Message;
using pipe_protocol::MessageType;
using pipe_protocol::PayloadReader;
using pipe_protocol::PayloadWriter;

namespace
{
constexpr DWORD StatusPollInterval = 1000;
constexpr ULONGLONG PairingTimeout = pairing_protocol::SessionLifetimeMilliseconds + 10'000;
constexpr ULONGLONG MillisecondsPerMinute = 60'000;
constexpr wchar_t EndpointOption[] = L"--endpoint";

std::vector<std::wstring> RequestedEndpoints(int argc, wchar_t** argv)
{
    std::vector<std::wstring> endpoints;
    for (int index = 0; index + 1 < argc; ++index)
    {
        if (wcscmp(argv[index], EndpointOption) == 0)
        {
            endpoints.push_back(argv[++index]);
        }
    }
    return endpoints.empty() ? LocalIpv4Endpoints(pairing_protocol::DefaultPort) : endpoints;
}

std::optional<std::wstring> StartPairing(const std::wstring& sid, const std::vector<std::wstring>& endpoints)
{
    PayloadWriter request;
    request.WriteString(sid);
    request.WriteStringList(endpoints);
    const std::optional<Message> reply = ExchangeWithService(MessageType::StartPairing, request);
    if (!reply)
    {
        return std::nullopt;
    }
    PayloadReader reader(reply->payload);
    std::wstring qr;
    if (!ReadSuccess(reader) || !reader.ReadString(&qr))
    {
        return std::nullopt;
    }
    return qr;
}

std::optional<PairingStatus> QueryPairingStatus()
{
    const std::optional<Message> reply = ExchangeWithService(MessageType::PairingStatus, PayloadWriter());
    if (!reply)
    {
        return std::nullopt;
    }
    PayloadReader reader(reply->payload);
    int32_t stage = 0;
    PairingStatus status;
    if (!ReadSuccess(reader) || !reader.ReadInt32(&stage) || !reader.ReadString(&status.code) ||
        !reader.ReadString(&status.phoneName))
    {
        return std::nullopt;
    }
    status.stage = static_cast<PairingStage>(stage);
    return status;
}

void ConfirmOnPc(bool accepted)
{
    PayloadWriter request;
    request.WriteInt32(accepted ? 1 : 0);
    SendAndReport(MessageType::ConfirmPairing, request);
}

void ShowInstructions(const std::wstring& qr, const std::vector<std::wstring>& endpoints)
{
    fwprintf(stdout, L"Scanne ce code avec l'application PhoneUnlock (valable %llu min) :\n\n",
        pairing_protocol::SessionLifetimeMilliseconds / MillisecondsPerMinute);
    PrintQrCode(Utf8FromWide(qr));
    fwprintf(stdout, L"\nAdresses annoncées :");
    for (const std::wstring& endpoint : endpoints)
    {
        fwprintf(stdout, L" %s", endpoint.c_str());
    }
    fwprintf(stdout, L"\nEn attente du téléphone…\n");
}

int FollowPairing()
{
    bool askedOnPc = false;
    bool announcedPhoneStep = false;
    const ULONGLONG deadline = GetTickCount64() + PairingTimeout;
    while (GetTickCount64() < deadline)
    {
        Sleep(StatusPollInterval);
        const std::optional<PairingStatus> status = QueryPairingStatus();
        if (!status)
        {
            return exit_code::Failure;
        }
        switch (status->stage)
        {
        case PairingStage::AwaitingConfirmation:
            if (!askedOnPc)
            {
                askedOnPc = true;
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
            return exit_code::Success;
        case PairingStage::Failed:
            fwprintf(stderr, L"Association refusée ou expirée.\n");
            return exit_code::Failure;
        default:
            break;
        }
    }
    fwprintf(stderr, L"Délai dépassé.\n");
    return exit_code::Failure;
}
}

int RunPairCommand(int argc, wchar_t** argv)
{
    const std::optional<std::wstring> sid = CurrentUserSid();
    const std::vector<std::wstring> endpoints = RequestedEndpoints(argc, argv);
    if (!sid)
    {
        return exit_code::Failure;
    }
    if (endpoints.empty())
    {
        fwprintf(stderr, L"Aucune adresse réseau utilisable : précise --endpoint adresse:port.\n");
        return exit_code::Failure;
    }

    const std::optional<std::wstring> qr = StartPairing(*sid, endpoints);
    if (!qr)
    {
        return exit_code::Failure;
    }
    ShowInstructions(*qr, endpoints);
    return FollowPairing();
}
