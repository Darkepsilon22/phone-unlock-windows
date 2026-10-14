#include "unlock/unlock_protocol.h"
#include "pairing/pairing_protocol.h"

namespace unlock_protocol
{
namespace
{
constexpr char SignatureDomain[] = "PhoneUnlock-v1-authorize";
constexpr uint64_t AllowDecision = 1;

enum RequestField : uint64_t
{
    VersionField = 1,
    ActionField = 2,
    RequestIdField = 3,
    NonceField = 4,
    PcIdField = 5,
    PairingIdField = 6,
    AccountSidField = 7,
    ContextField = 8,
    IssuedAtField = 9,
    ExpiresAtField = 10,
    PcNameField = 11,
    AccountNameField = 12,
    DecisionField = 13,
};

cbor::Map Fields(const AuthorizationRequest& request)
{
    return {
        { VersionField, cbor::UInt(pairing_protocol::ProtocolVersion) },
        { ActionField, cbor::UInt(static_cast<uint64_t>(request.action)) },
        { RequestIdField, cbor::ByteString(request.requestId) },
        { NonceField, cbor::ByteString(request.nonce) },
        { PcIdField, cbor::ByteString(request.pcId) },
        { PairingIdField, cbor::ByteString(request.pairingId) },
        { AccountSidField, cbor::Text(request.accountSid) },
        { ContextField, cbor::ByteString({ request.context.begin(), request.context.end() }) },
        { IssuedAtField, cbor::UInt(request.issuedAt) },
        { ExpiresAtField, cbor::UInt(request.expiresAt) },
        { PcNameField, cbor::Text(request.pcName) },
        { AccountNameField, cbor::Text(request.accountName) },
    };
}
}

cbor::Value Encode(const AuthorizationRequest& request)
{
    return cbor::Object(Fields(request));
}

cbor::Bytes SignedMessage(const AuthorizationRequest& request)
{
    cbor::Map authorization = Fields(request);
    authorization.emplace_back(DecisionField, cbor::UInt(AllowDecision));
    const cbor::Bytes encoded = cbor::Encode(cbor::Object(std::move(authorization)));

    cbor::Bytes message(SignatureDomain, SignatureDomain + std::char_traits<char>::length(SignatureDomain));
    message.push_back(0);
    message.insert(message.end(), encoded.begin(), encoded.end());
    return message;
}
}
