#pragma once

#include "encoding/cbor.h"
#include "crypto/crypto.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pairing_protocol
{
constexpr uint64_t ProtocolVersion = 1;
constexpr size_t TokenSize = 32;
constexpr size_t PairingIdSize = 16;
constexpr uint16_t DefaultPort = 47863;

struct Endpoint
{
    std::string host;
    uint16_t port = DefaultPort;
};

std::optional<Endpoint> ParseEndpoint(const std::string& text);

std::string QrText(const cbor::Bytes& pcId, const crypto::Digest& pcFingerprint, const cbor::Bytes& token,
    const std::vector<Endpoint>& endpoints);

std::optional<crypto::Digest> Transcript(const cbor::Bytes& pcId, const crypto::Digest& pcFingerprint,
    const crypto::Digest& transportFingerprint, const crypto::Digest& authorizationFingerprint,
    const cbor::Bytes& token, const cbor::Bytes& pairingId);

std::wstring ComparisonCode(const crypto::Digest& transcript);

cbor::Bytes ConfirmationMessage(const crypto::Digest& transcript);
}
