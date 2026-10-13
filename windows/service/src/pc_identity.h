#pragma once

#include "cbor.h"
#include "crypto.h"
#include <optional>
#include <string>

struct PcIdentity
{
    cbor::Bytes pcId;
    crypto::Digest fingerprint = {};
    std::string name;
};

std::optional<PcIdentity> LoadPcIdentity(const std::wstring& stateDirectory);
