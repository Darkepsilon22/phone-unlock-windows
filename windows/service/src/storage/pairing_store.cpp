#include "storage/pairing_store.h"
#include "storage/file_io.h"
#include "log.h"
#include "encoding/text.h"
#include <algorithm>

namespace
{
constexpr wchar_t PairingPattern[] = L"*.cbor";
constexpr wchar_t PairingExtension[] = L".cbor";
constexpr DWORD MaxPairingFileSize = 8 * 1024;

enum PairingField : uint64_t
{
    IdField = 1,
    SidField = 2,
    PhoneNameField = 3,
    TransportField = 4,
    AuthorizationField = 5,
    CreatedAtField = 6,
};

cbor::Bytes Serialize(const Pairing& pairing)
{
    return cbor::Encode(cbor::Object({
        { IdField, cbor::ByteString(pairing.pairingId) },
        { SidField, cbor::Text(Utf8FromWide(pairing.accountSid)) },
        { PhoneNameField, cbor::Text(pairing.phoneName) },
        { TransportField, cbor::ByteString({ pairing.transportFingerprint.begin(), pairing.transportFingerprint.end() }) },
        { AuthorizationField, cbor::ByteString(pairing.authorizationSpki) },
        { CreatedAtField, cbor::UInt(pairing.createdAt) },
    }));
}

std::optional<Pairing> Deserialize(const std::vector<uint8_t>& content)
{
    const std::optional<cbor::Value> value = cbor::Decode(content);
    const cbor::Map* map = cbor::AsMap(value ? &*value : nullptr);
    if (!map)
    {
        return std::nullopt;
    }
    const cbor::Bytes* id = cbor::AsBytes(cbor::Find(*map, IdField));
    const std::string* sid = cbor::AsText(cbor::Find(*map, SidField));
    const std::string* phoneName = cbor::AsText(cbor::Find(*map, PhoneNameField));
    const cbor::Bytes* transport = cbor::AsBytes(cbor::Find(*map, TransportField));
    const cbor::Bytes* authorization = cbor::AsBytes(cbor::Find(*map, AuthorizationField));
    const std::optional<uint64_t> createdAt = cbor::AsUInt(cbor::Find(*map, CreatedAtField));
    if (!id || !sid || !phoneName || !transport || transport->size() != crypto::Sha256Size || !authorization ||
        !createdAt)
    {
        return std::nullopt;
    }

    Pairing pairing;
    pairing.pairingId = *id;
    pairing.accountSid = WideFromUtf8(*sid);
    pairing.phoneName = *phoneName;
    std::copy(transport->begin(), transport->end(), pairing.transportFingerprint.begin());
    pairing.authorizationSpki = *authorization;
    pairing.createdAt = *createdAt;
    return pairing;
}
}

PairingStore::PairingStore(std::wstring directory) : directory_(std::move(directory))
{
    LoadAll();
}

HRESULT PairingStore::Save(const Pairing& pairing)
{
    const HRESULT hr = WriteFileAtomically(PathFor(pairing.pairingId), Serialize(pairing));
    if (SUCCEEDED(hr))
    {
        std::lock_guard lock(mutex_);
        pairings_.push_back(pairing);
    }
    return hr;
}

std::optional<Pairing> PairingStore::FindByTransport(const crypto::Digest& fingerprint)
{
    std::lock_guard lock(mutex_);
    const auto match = std::find_if(pairings_.begin(), pairings_.end(), [&fingerprint](const Pairing& pairing) {
        return crypto::ConstantTimeEquals(pairing.transportFingerprint, fingerprint);
    });
    return match == pairings_.end() ? std::nullopt : std::optional<Pairing>(*match);
}

std::optional<Pairing> PairingStore::FindByAccount(const std::wstring& sid)
{
    std::lock_guard lock(mutex_);
    const auto match = std::find_if(pairings_.rbegin(), pairings_.rend(),
        [&sid](const Pairing& pairing) { return pairing.accountSid == sid; });
    return match == pairings_.rend() ? std::nullopt : std::optional<Pairing>(*match);
}

size_t PairingStore::Count()
{
    std::lock_guard lock(mutex_);
    return pairings_.size();
}

void PairingStore::LoadAll()
{
    for (const std::wstring& path : ListFiles(directory_, PairingPattern))
    {
        std::vector<uint8_t> content;
        std::optional<Pairing> pairing;
        if (SUCCEEDED(ReadSmallFile(path, MaxPairingFileSize, &content)))
        {
            pairing = Deserialize(content);
        }
        if (pairing)
        {
            pairings_.push_back(std::move(*pairing));
        }
        else
        {
            LogLine(L"pairing file ignored");
        }
    }
    LogLine(L"pairings loaded=%zu", pairings_.size());
}

std::wstring PairingStore::PathFor(const cbor::Bytes& pairingId) const
{
    return directory_ + L"\\" + HexFromBytes(pairingId) + PairingExtension;
}
