#include "ipc/admin_requests.h"
#include "encoding/text.h"
#include "log.h"

using pipe_protocol::Message;
using pipe_protocol::MessageType;
using pipe_protocol::PayloadReader;
using pipe_protocol::PayloadWriter;

namespace
{
PayloadWriter ResultOnly(HRESULT hr)
{
    PayloadWriter writer;
    writer.WriteInt32(hr);
    return writer;
}

std::optional<std::vector<pairing_protocol::Endpoint>> ParseEndpoints(const std::vector<std::wstring>& texts)
{
    std::vector<pairing_protocol::Endpoint> endpoints;
    for (const std::wstring& text : texts)
    {
        std::optional<pairing_protocol::Endpoint> endpoint = pairing_protocol::ParseEndpoint(Utf8FromWide(text));
        if (!endpoint)
        {
            return std::nullopt;
        }
        endpoints.push_back(std::move(*endpoint));
    }
    return endpoints;
}
}

AdminRequests::AdminRequests(const SecretStore& secrets, AuthorizationBroker& broker, PairingManager* pairing)
    : secrets_(secrets), broker_(broker), pairing_(pairing)
{
}

PayloadWriter AdminRequests::Handle(const Message& request)
{
    LogLine(L"administrator request type=%u", static_cast<unsigned>(request.type));
    switch (request.type)
    {
    case MessageType::Provision:
        return Provision(request);
    case MessageType::Authorize:
        return Authorize(request);
    case MessageType::StartPairing:
        return StartPairing(request);
    case MessageType::PairingStatus:
        return QueryPairingStatus();
    case MessageType::ConfirmPairing:
        return ConfirmPairing(request);
    default:
        return ResultOnly(E_INVALIDARG);
    }
}

PayloadWriter AdminRequests::Provision(const Message& request)
{
    PayloadReader reader(request.payload);
    std::wstring sid;
    SecureString password;
    HRESULT hr = E_INVALIDARG;
    if (reader.ReadString(&sid) && reader.ReadSecret(&password) && reader.AtEnd())
    {
        hr = secrets_.Save(sid, password);
    }
    LogLine(L"provision hr=0x%08X", static_cast<unsigned>(hr));
    return ResultOnly(hr);
}

PayloadWriter AdminRequests::Authorize([[maybe_unused]] const Message& request)
{
#ifdef PHONEUNLOCK_TEST_COMMANDS
    PayloadReader reader(request.payload);
    std::wstring sid;
    HRESULT hr = E_INVALIDARG;
    if (reader.ReadString(&sid) && reader.AtEnd() && IsValidSid(sid))
    {
        broker_.Authorize(sid);
        hr = S_OK;
    }
    LogLine(L"test authorization hr=0x%08X", static_cast<unsigned>(hr));
    return ResultOnly(hr);
#else
    LogLine(L"test authorization refused");
    return ResultOnly(E_ACCESSDENIED);
#endif
}

PayloadWriter AdminRequests::StartPairing(const Message& request)
{
    if (!pairing_)
    {
        return ResultOnly(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
    }
    PayloadReader reader(request.payload);
    std::wstring sid;
    std::vector<std::wstring> texts;
    if (!reader.ReadString(&sid) || !reader.ReadStringList(&texts) || !reader.AtEnd() || !IsValidSid(sid))
    {
        return ResultOnly(E_INVALIDARG);
    }
    const std::optional<std::vector<pairing_protocol::Endpoint>> endpoints = ParseEndpoints(texts);
    if (!endpoints)
    {
        return ResultOnly(E_INVALIDARG);
    }

    const std::optional<std::string> qr = pairing_->Start(sid, *endpoints);
    if (!qr)
    {
        return ResultOnly(E_FAIL);
    }
    PayloadWriter reply;
    reply.WriteInt32(S_OK);
    reply.WriteString(WideFromUtf8(*qr));
    return reply;
}

PayloadWriter AdminRequests::QueryPairingStatus()
{
    if (!pairing_)
    {
        return ResultOnly(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
    }
    const PairingStatus status = pairing_->Status();
    PayloadWriter reply;
    reply.WriteInt32(S_OK);
    reply.WriteInt32(static_cast<int32_t>(status.stage));
    reply.WriteString(status.code);
    reply.WriteString(status.phoneName);
    return reply;
}

PayloadWriter AdminRequests::ConfirmPairing(const Message& request)
{
    PayloadReader reader(request.payload);
    int32_t accepted = 0;
    if (!pairing_ || !reader.ReadInt32(&accepted) || !reader.AtEnd())
    {
        return ResultOnly(E_INVALIDARG);
    }
    return ResultOnly(pairing_->ConfirmOnPc(accepted != 0) ? S_OK : E_ILLEGAL_STATE_CHANGE);
}
