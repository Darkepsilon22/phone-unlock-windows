#pragma once

#include "encoding/cbor.h"
#include "crypto/crypto.h"
#include <optional>
#include <string>

struct PcIdentity
{
    cbor::Bytes pcId;
    crypto::Digest fingerprint = {};
    std::string name;
};

std::optional<PcIdentity> LoadPcIdentity(const std::wstring& stateDirectory);
