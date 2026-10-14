#include "unlock/unlock_requests.h"
#include "crypto/crypto.h"
#include "encoding/text.h"
#include "log.h"
#include "network/api_errors.h"
#include <ctime>

using api_errors::BadRequest;
using api_errors::Conflict;
using api_errors::Gone;
using api_errors::RateLimited;

namespace
{
constexpr ULONGLONG RequestLifetime = 30'000;
constexpr uint64_t RequestLifetimeSeconds = RequestLifetime / 1000;
constexpr ULONGLONG QuotaWindow = 60'000;
constexpr size_t MaxRequestsPerWindow = 3;

enum ProofField : uint64_t
{
    ProofRequestIdField = 1,
    SignatureField = 2,
};

enum ResultField : uint64_t
{
    StateField = 1,
    ErrorField = 2,
};
}

UnlockRequests::UnlockRequests(LogonTiles& tiles, cbor::Bytes pcId, std::string pcName, AccountNamer accountName,
    MonotonicClock clock, WallClock unixSeconds)
    : tiles_(tiles),
      pcId_(std::move(pcId)),
      pcName_(std::move(pcName)),
      accountName_(std::move(accountName)),
      clock_(std::move(clock)),
      unixSeconds_(std::move(unixSeconds))
{
}

HttpResponse UnlockRequests::Create(const Pairing& pairing)
{
    const std::optional<TileView> tile = tiles_.Find(pairing.accountSid);
    if (!tile)
    {
        return api_errors::Error(Conflict, "WRONG_CONTEXT");
    }
    const std::optional<std::vector<uint8_t>> requestId = crypto::RandomBytes(unlock_protocol::RequestIdSize);
    const std::optional<std::vector<uint8_t>> nonce = crypto::RandomBytes(unlock_protocol::NonceSize);
    if (!requestId || !nonce)
    {
        return api_errors::Error(Gone, "EXPIRED");
    }

    Entry entry;
    entry.request.action = tile->scenario == TileScenario::Unlock ? unlock_protocol::Action::Unlock
                                                                 : unlock_protocol::Action::InteractiveLogon;
    entry.request.requestId = *requestId;
    entry.request.nonce = *nonce;
    entry.request.pcId = pcId_;
    entry.request.pairingId = pairing.pairingId;
    entry.request.accountSid = Utf8FromWide(pairing.accountSid);
    entry.request.context = tile->context;
    entry.request.issuedAt = unixSeconds_();
    entry.request.expiresAt = entry.request.issuedAt + RequestLifetimeSeconds;
    entry.request.pcName = pcName_;
    entry.request.accountName = accountName_(pairing.accountSid);
    entry.phoneName = pairing.phoneName;

    std::lock_guard lock(mutex_);
    if (!WithinQuota(pairing.accountSid))
    {
        return api_errors::Error(RateLimited, "RATE_LIMITED");
    }
    entry.deadline = clock_() + RequestLifetime;
    creations_[pairing.accountSid].push_back(clock_());
    const cbor::Value body = unlock_protocol::Encode(entry.request);
    active_.insert_or_assign(pairing.accountSid, std::move(entry));
    changed_.notify_all();
    LogLine(L"history unlock requested phone=%s", WideFromUtf8(pairing.phoneName).c_str());
    return api_errors::Body(body);
}

HttpResponse UnlockRequests::Prove(const Pairing& pairing, const cbor::Bytes& requestId, const cbor::Bytes& body)
{
    const std::optional<cbor::Value> decoded = cbor::Decode(body);
    const cbor::Map* fields = cbor::AsMap(decoded ? &*decoded : nullptr);
    const cbor::Bytes* proofId = fields ? cbor::AsBytes(cbor::Find(*fields, ProofRequestIdField)) : nullptr;
    const cbor::Bytes* signature = fields ? cbor::AsBytes(cbor::Find(*fields, SignatureField)) : nullptr;
    if (!fields || fields->size() != 2 || !proofId || *proofId != requestId || !signature ||
        signature->size() != crypto::EcdsaP256SignatureSize)
    {
        return api_errors::Error(BadRequest, "INVALID_PROOF");
    }

    std::lock_guard lock(mutex_);
    Entry* entry = EntryFor(pairing, requestId);
    if (!entry || entry->stage != Stage::Presented)
    {
        return api_errors::Error(Gone, entry && entry->error ? entry->error : "EXPIRED");
    }
    if (clock_() >= entry->deadline)
    {
        return Terminate(*entry, Gone, "EXPIRED");
    }
    const std::optional<TileView> tile = tiles_.Find(pairing.accountSid);
    if (!tile || tile->context != entry->request.context)
    {
        return Terminate(*entry, Conflict, "WRONG_CONTEXT");
    }
    if (!crypto::VerifyEcdsaP256(pairing.authorizationSpki, unlock_protocol::SignedMessage(entry->request), *signature))
    {
        return Terminate(*entry, BadRequest, "INVALID_PROOF");
    }

    entry->stage = Stage::Consumed;
    if (!tiles_.Authorize(pairing.accountSid, entry->request.context))
    {
        return Terminate(*entry, Conflict, "WRONG_CONTEXT");
    }
    changed_.notify_all();
    LogLine(L"history unlock authorized phone=%s", WideFromUtf8(entry->phoneName).c_str());
    return StageBody(*entry);
}

HttpResponse UnlockRequests::Result(const Pairing& pairing, const cbor::Bytes& requestId, std::chrono::milliseconds wait)
{
    std::unique_lock lock(mutex_);
    Entry* entry = EntryFor(pairing, requestId);
    if (!entry)
    {
        return api_errors::Error(Gone, "EXPIRED");
    }
    changed_.wait_for(lock, wait, [this, &pairing, &requestId] {
        const Entry* current = EntryFor(pairing, requestId);
        return !current || IsFinal(current->stage);
    });
    entry = EntryFor(pairing, requestId);
    return entry ? StageBody(*entry) : api_errors::Error(Gone, "CANCELLED");
}

void UnlockRequests::ReportLogonResult(const std::wstring& sid, LONG status)
{
    {
        std::lock_guard lock(mutex_);
        const auto match = active_.find(sid);
        if (match == active_.end() || match->second.stage != Stage::Consumed)
        {
            return;
        }
        Entry& entry = match->second;
        entry.stage = status == 0 ? Stage::LogonSucceeded : Stage::LogonRejected;
        entry.error = status == 0 ? nullptr : "WINDOWS_REJECTED";
        LogLine(L"history unlock result phone=%s status=0x%08X", WideFromUtf8(entry.phoneName).c_str(),
            static_cast<unsigned>(status));
    }
    changed_.notify_all();
}

uint64_t UnlockRequests::DefaultUnixSeconds()
{
    return static_cast<uint64_t>(std::time(nullptr));
}

bool UnlockRequests::IsFinal(Stage stage)
{
    return stage == Stage::LogonSucceeded || stage == Stage::LogonRejected || stage == Stage::Terminated;
}

HttpResponse UnlockRequests::StageBody(const Entry& entry)
{
    static constexpr const char* Names[] = { "presented", "consumed", "logon_succeeded", "logon_rejected", "terminated" };
    cbor::Map body = { { StateField, cbor::Text(Names[static_cast<int>(entry.stage)]) } };
    if (entry.error)
    {
        body.emplace_back(ErrorField, cbor::Text(entry.error));
    }
    return api_errors::Body(cbor::Object(std::move(body)));
}

UnlockRequests::Entry* UnlockRequests::EntryFor(const Pairing& pairing, const cbor::Bytes& requestId)
{
    const auto match = active_.find(pairing.accountSid);
    if (match == active_.end() || !crypto::ConstantTimeEquals(match->second.request.requestId, requestId) ||
        !crypto::ConstantTimeEquals(match->second.request.pairingId, pairing.pairingId))
    {
        return nullptr;
    }
    return &match->second;
}

bool UnlockRequests::WithinQuota(const std::wstring& sid)
{
    std::deque<ULONGLONG>& times = creations_[sid];
    const ULONGLONG now = clock_();
    while (!times.empty() && now - times.front() >= QuotaWindow)
    {
        times.pop_front();
    }
    return times.size() < MaxRequestsPerWindow;
}

HttpResponse UnlockRequests::Terminate(Entry& entry, USHORT status, const char* error)
{
    entry.stage = Stage::Terminated;
    entry.error = error;
    changed_.notify_all();
    LogLine(L"history unlock refused phone=%s reason=%S", WideFromUtf8(entry.phoneName).c_str(), error);
    return api_errors::Error(status, error);
}
