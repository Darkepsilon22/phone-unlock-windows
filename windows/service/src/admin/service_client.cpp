#include "admin/service_client.h"
#include "pipe_io.h"
#include <cstdio>

using pipe_protocol::Message;
using pipe_protocol::MessageType;
using pipe_protocol::PayloadReader;
using pipe_protocol::PayloadWriter;

namespace
{
constexpr DWORD ConnectTimeout = 3000;

void ReportConnectionFailure(DWORD error)
{
    if (error == ERROR_ACCESS_DENIED)
    {
        fwprintf(stderr, L"Accès refusé : lance la commande dans un terminal administrateur.\n");
        return;
    }
    fwprintf(stderr, L"Service injoignable (erreur %lu). Est-il démarré ?\n", error);
}
}

std::optional<Message> ExchangeWithService(MessageType type, const PayloadWriter& request)
{
    HANDLE pipe = pipe_protocol::ConnectToService(pipe_protocol::AdminPipeName, nullptr, ConnectTimeout);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        ReportConnectionFailure(GetLastError());
        return std::nullopt;
    }

    std::optional<Message> reply;
    if (pipe_protocol::WriteMessage(pipe, type, request.Bytes(), nullptr))
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

bool ReadSuccess(PayloadReader& reply)
{
    int32_t hr = E_FAIL;
    if (!reply.ReadInt32(&hr))
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

int SendAndReport(MessageType type, const PayloadWriter& request)
{
    const std::optional<Message> reply = ExchangeWithService(type, request);
    if (!reply)
    {
        return exit_code::Failure;
    }
    PayloadReader reader(reply->payload);
    if (!ReadSuccess(reader))
    {
        return exit_code::Failure;
    }
    fwprintf(stdout, L"OK\n");
    return exit_code::Success;
}
