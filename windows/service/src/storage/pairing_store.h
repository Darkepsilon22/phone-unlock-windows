#pragma once

#include "encoding/cbor.h"
#include "crypto/crypto.h"
#include <windows.h>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct Pairing
{
    cbor::Bytes pairingId;
    std::wstring accountSid;
    std::string phoneName;
    crypto::Digest transportFingerprint = {};
    cbor::Bytes authorizationSpki;
    uint64_t createdAt = 0;
};

class PairingStore
{
public:
    explicit PairingStore(std::wstring directory);

    HRESULT Save(const Pairing& pairing);
    std::optional<Pairing> FindByTransport(const crypto::Digest& fingerprint);
    std::optional<Pairing> FindByAccount(const std::wstring& sid);
    size_t Count();

private:
    void LoadAll();
    std::wstring PathFor(const cbor::Bytes& pairingId) const;

    std::wstring directory_;
    std::mutex mutex_;
    std::vector<Pairing> pairings_;
};
