#pragma once

#include "encoding/cbor.h"
#include "ipc/logon_tiles.h"
#include <cstdint>
#include <string>

namespace unlock_protocol
{
constexpr size_t RequestIdSize = 16;
constexpr size_t NonceSize = 32;

enum class Action : uint64_t
{
    InteractiveLogon = 1,
    Unlock = 2,
};

struct AuthorizationRequest
{
    Action action = Action::Unlock;
    cbor::Bytes requestId;
    cbor::Bytes nonce;
    cbor::Bytes pcId;
    cbor::Bytes pairingId;
    std::string accountSid;
    ContextId context = {};
    uint64_t issuedAt = 0;
    uint64_t expiresAt = 0;
    std::string pcName;
    std::string accountName;
};

cbor::Value Encode(const AuthorizationRequest& request);

cbor::Bytes SignedMessage(const AuthorizationRequest& request);
}
