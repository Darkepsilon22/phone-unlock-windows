#include "pairing/pairing_manager.h"
#include "network/api_errors.h"
#include "log.h"
#include "encoding/text.h"
#include <sddl.h>
#include <algorithm>
#include <ctime>

using api_errors::BadRequest;
using api_errors::Conflict;
using api_errors::Forbidden;
using api_errors::Gone;

namespace
{
constexpr int MaxAttempts = 3;
constexpr size_t MaxNameCharacters = 64;
constexpr char PairedState[] = "paired";

enum PairRequestField : uint64_t
{
    TokenField = 1,
    AuthorizationKeyField = 2,
    PhoneNameField = 3,
    AttestationField = 4,
};

enum PairChallengeField : uint64_t
{
    PairingIdField = 1,
    PcNameField = 2,
    AccountNameField = 3,
};

enum PairConfirmField : uint64_t
{
    ConfirmPairingIdField = 1,
    SignatureField = 2,
};

enum PairResultField : uint64_t
{
    StateField = 1,
};

std::string AccountDisplayName(const std::wstring& sid)
{
    PSID binary = nullptr;
    if (!ConvertStringSidToSidW(sid.c_str(), &binary))
    {
        return "compte";
    }
    wchar_t name[256] = {};
    wchar_t domain[256] = {};
    DWORD nameSize = 256;
    DWORD domainSize = 256;
    SID_NAME_USE use = SidTypeUnknown;
    const BOOL found = LookupAccountSidW(nullptr, binary, name, &nameSize, domain, &domainSize, &use);
    LocalFree(binary);
    return found ? SanitizedDisplayName(Utf8FromWide(name), MaxNameCharacters) : "compte";
}

bool HasOnlyKeys(const cbor::Map& map, std::initializer_list<uint64_t> allowed)
{
    for (const auto& [key, value] : map)
    {
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end())
        {
            return false;
        }
    }
    return true;
}

const cbor::Map* DecodeMap(const HttpRequest& request, std::optional<cbor::Value>& holder)
{
    holder = cbor::Decode(request.body);
    return cbor::AsMap(holder ? &*holder : nullptr);
}
}

PairingManager::PairingManager(PcIdentity identity, PairingStore& store, MonotonicClock clock)
    : identity_(std::move(identity)), store_(store), clock_(std::move(clock))
{
}

std::optional<std::string> PairingManager::Start(const std::wstring& accountSid,
    const std::vector<pairing_protocol::Endpoint>& endpoints)
{
    std::optional<std::vector<uint8_t>> token = crypto::RandomBytes(pairing_protocol::TokenSize);
    if (!token || endpoints.empty())
    {
        return std::nullopt;
    }

    std::lock_guard lock(mutex_);
    session_ = Session{};
    session_.accountSid = accountSid;
    session_.token = *token;
    session_.expiresAt = clock_() + pairing_protocol::SessionLifetimeMilliseconds;
    session_.attemptsLeft = MaxAttempts;
    session_.stage = PairingStage::WaitingForPhone;
    LogLine(L"pairing started");
    return pairing_protocol::QrText(identity_.pcId, identity_.fingerprint, session_.token, endpoints);
}

PairingStatus PairingManager::Status()
{
    std::lock_guard lock(mutex_);
    ExpireIfNeeded();
    const bool codeVisible = session_.stage == PairingStage::AwaitingConfirmation ||
        session_.stage == PairingStage::PcConfirmed;
    return { session_.stage, codeVisible ? pairing_protocol::ComparisonCode(session_.transcript) : std::wstring(),
        WideFromUtf8(session_.phoneName) };
}

bool PairingManager::ConfirmOnPc(bool accepted)
{
    std::lock_guard lock(mutex_);
    ExpireIfNeeded();
    if (session_.stage != PairingStage::AwaitingConfirmation)
    {
        return false;
    }
    if (!accepted)
    {
        Fail(L"rejected on pc");
        return true;
    }
    session_.stage = PairingStage::PcConfirmed;
    LogLine(L"pairing confirmed on pc");
    return true;
}

HttpResponse PairingManager::HandlePairRequest(const HttpRequest& request)
{
    std::optional<cbor::Value> holder;
    const cbor::Map* body = DecodeMap(request, holder);
    const cbor::Bytes* token = body ? cbor::AsBytes(cbor::Find(*body, TokenField)) : nullptr;
    const cbor::Bytes* authorizationSpki = body ? cbor::AsBytes(cbor::Find(*body, AuthorizationKeyField)) : nullptr;
    const std::string* phoneName = body ? cbor::AsText(cbor::Find(*body, PhoneNameField)) : nullptr;
    if (!request.clientFingerprint || !body ||
        !HasOnlyKeys(*body, { TokenField, AuthorizationKeyField, PhoneNameField, AttestationField }) || !token ||
        !authorizationSpki || !crypto::IsEcdsaP256Spki(*authorizationSpki) || !phoneName)
    {
        return api_errors::Error(BadRequest, "INVALID_PROOF");
    }
    const std::string cleanName = SanitizedDisplayName(*phoneName, MaxNameCharacters);
    if (cleanName.empty())
    {
        return api_errors::Error(BadRequest, "INVALID_PROOF");
    }

    std::lock_guard lock(mutex_);
    ExpireIfNeeded();
    if (session_.stage != PairingStage::WaitingForPhone)
    {
        return api_errors::Error(Gone, "EXPIRED");
    }
    if (!crypto::ConstantTimeEquals(*token, session_.token))
    {
        if (--session_.attemptsLeft <= 0)
        {
            Fail(L"too many attempts");
        }
        return api_errors::Error(Forbidden, "INVALID_PROOF");
    }

    const std::optional<std::vector<uint8_t>> pairingId = crypto::RandomBytes(pairing_protocol::PairingIdSize);
    const std::optional<crypto::Digest> authorizationFingerprint = crypto::Sha256(*authorizationSpki);
    std::optional<crypto::Digest> transcript;
    if (pairingId && authorizationFingerprint)
    {
        transcript = pairing_protocol::Transcript(identity_.pcId, identity_.fingerprint, *request.clientFingerprint,
            *authorizationFingerprint, session_.token, *pairingId);
    }
    if (!transcript)
    {
        Fail(L"internal error");
        return api_errors::Error(Gone, "EXPIRED");
    }

    session_.pairingId = *pairingId;
    session_.transportFingerprint = *request.clientFingerprint;
    session_.authorizationSpki = *authorizationSpki;
    session_.phoneName = cleanName;
    session_.transcript = *transcript;
    session_.stage = PairingStage::AwaitingConfirmation;
    LogLine(L"pairing challenged");

    return api_errors::Body(cbor::Object({
        { PairingIdField, cbor::ByteString(session_.pairingId) },
        { PcNameField, cbor::Text(identity_.name) },
        { AccountNameField, cbor::Text(AccountDisplayName(session_.accountSid)) },
    }));
}

HttpResponse PairingManager::HandlePairConfirm(const HttpRequest& request)
{
    std::optional<cbor::Value> holder;
    const cbor::Map* body = DecodeMap(request, holder);
    const cbor::Bytes* pairingId = body ? cbor::AsBytes(cbor::Find(*body, ConfirmPairingIdField)) : nullptr;
    const cbor::Bytes* signature = body ? cbor::AsBytes(cbor::Find(*body, SignatureField)) : nullptr;
    if (!request.clientFingerprint || !body || !HasOnlyKeys(*body, { ConfirmPairingIdField, SignatureField }) ||
        !pairingId || !signature)
    {
        return api_errors::Error(BadRequest, "INVALID_PROOF");
    }

    std::lock_guard lock(mutex_);
    ExpireIfNeeded();
    const bool samePhone = crypto::ConstantTimeEquals(*request.clientFingerprint, session_.transportFingerprint) &&
        crypto::ConstantTimeEquals(*pairingId, session_.pairingId);
    if (session_.stage == PairingStage::AwaitingConfirmation && samePhone)
    {
        return api_errors::Error(Conflict, "WRONG_CONTEXT");
    }
    if (session_.stage != PairingStage::PcConfirmed || !samePhone)
    {
        return api_errors::Error(Gone, "EXPIRED");
    }
    if (!crypto::VerifyEcdsaP256(session_.authorizationSpki, pairing_protocol::ConfirmationMessage(session_.transcript),
            *signature))
    {
        Fail(L"invalid signature");
        return api_errors::Error(BadRequest, "INVALID_PROOF");
    }

    Pairing pairing;
    pairing.pairingId = session_.pairingId;
    pairing.accountSid = session_.accountSid;
    pairing.phoneName = session_.phoneName;
    pairing.transportFingerprint = session_.transportFingerprint;
    pairing.authorizationSpki = session_.authorizationSpki;
    pairing.createdAt = static_cast<uint64_t>(std::time(nullptr));
    const HRESULT hr = store_.Save(pairing);
    if (FAILED(hr))
    {
        LogLine(L"pairing save failed hr=0x%08X", static_cast<unsigned>(hr));
        Fail(L"save failed");
        return api_errors::Error(Gone, "EXPIRED");
    }

    session_.stage = PairingStage::Completed;
    session_.token.assign(session_.token.size(), 0);
    LogLine(L"pairing completed");
    return api_errors::Body(cbor::Object({ { StateField, cbor::Text(PairedState) } }));
}

void PairingManager::ExpireIfNeeded()
{
    const bool pending = session_.stage == PairingStage::WaitingForPhone ||
        session_.stage == PairingStage::AwaitingConfirmation || session_.stage == PairingStage::PcConfirmed;
    if (pending && clock_() >= session_.expiresAt)
    {
        Fail(L"expired");
    }
}

void PairingManager::Fail(const wchar_t* reason)
{
    session_.stage = PairingStage::Failed;
    session_.token.assign(session_.token.size(), 0);
    LogLine(L"pairing failed reason=%s", reason);
}
