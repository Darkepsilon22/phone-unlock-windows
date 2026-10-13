#pragma once

#include "network/http_server.h"
#include "pairing/pairing_protocol.h"
#include "storage/pairing_store.h"
#include "support/clock.h"
#include "pairing/pc_identity.h"
#include <mutex>
#include <optional>
#include <string>
#include <vector>

enum class PairingStage : int32_t
{
    Idle = 0,
    WaitingForPhone = 1,
    AwaitingConfirmation = 2,
    PcConfirmed = 3,
    Completed = 4,
    Failed = 5,
};

struct PairingStatus
{
    PairingStage stage = PairingStage::Idle;
    std::wstring code;
    std::wstring phoneName;
};

class PairingManager
{
public:
    PairingManager(PcIdentity identity, PairingStore& store, MonotonicClock clock = SystemMilliseconds);

    std::optional<std::string> Start(const std::wstring& accountSid,
        const std::vector<pairing_protocol::Endpoint>& endpoints);
    PairingStatus Status();
    bool ConfirmOnPc(bool accepted);

    HttpResponse HandlePairRequest(const HttpRequest& request);
    HttpResponse HandlePairConfirm(const HttpRequest& request);

private:
    struct Session
    {
        std::wstring accountSid;
        cbor::Bytes token;
        ULONGLONG expiresAt = 0;
        int attemptsLeft = 0;
        PairingStage stage = PairingStage::Idle;
        cbor::Bytes pairingId;
        crypto::Digest transportFingerprint = {};
        cbor::Bytes authorizationSpki;
        std::string phoneName;
        crypto::Digest transcript = {};
    };

    void ExpireIfNeeded();
    void Fail(const wchar_t* reason);

    PcIdentity identity_;
    PairingStore& store_;
    MonotonicClock clock_;
    std::mutex mutex_;
    Session session_;
};
