#pragma once

#include "ipc/logon_tiles.h"
#include "network/http_server.h"
#include "storage/pairing_store.h"
#include "support/change_feed.h"
#include "support/clock.h"
#include "unlock/unlock_protocol.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <mutex>
#include <string>

struct TileRequest
{
    using Outcome = pipe_protocol::TileRequestOutcome;

    Outcome outcome = Outcome::WrongContext;
    std::string matchCode;
};

class UnlockRequests
{
public:
    using AccountNamer = std::function<std::string(const std::wstring& sid)>;
    using WallClock = std::function<uint64_t()>;

    UnlockRequests(LogonTiles& tiles, PairingStore& pairings, ChangeFeed& changes, cbor::Bytes pcId, std::string pcName,
        AccountNamer accountName, MonotonicClock clock = SystemMilliseconds, WallClock unixSeconds = DefaultUnixSeconds);

    HttpResponse Create(const Pairing& pairing);
    TileRequest CreateFromTile(const std::wstring& sid, const ContextId& context);
    HttpResponse Active(const Pairing& pairing);
    HttpResponse Prove(const Pairing& pairing, const cbor::Bytes& requestId, const cbor::Bytes& body);
    HttpResponse Result(const Pairing& pairing, const cbor::Bytes& requestId, std::chrono::milliseconds wait);
    void ReportLogonResult(const std::wstring& sid, LONG status);
    void Close();

private:
    enum class Stage
    {
        Presented,
        Consumed,
        LogonSucceeded,
        LogonRejected,
        Terminated,
    };

    struct Entry
    {
        unlock_protocol::AuthorizationRequest request;
        std::string phoneName;
        ULONGLONG lifetime = 0;
        ULONGLONG deadline = 0;
        Stage stage = Stage::Presented;
        const char* error = nullptr;
    };

    static uint64_t DefaultUnixSeconds();
    static bool IsFinal(Stage stage);
    static HttpResponse StageBody(const Entry& entry);

    std::optional<Entry> NewEntry(const Pairing& pairing, const TileView& tile, ULONGLONG lifetime);
    bool Store(const std::wstring& sid, Entry entry);
    Entry* EntryFor(const Pairing& pairing, const cbor::Bytes& requestId);
    bool WithinQuota(const std::wstring& sid);
    HttpResponse Terminate(Entry& entry, USHORT status, const char* error);

    LogonTiles& tiles_;
    PairingStore& pairings_;
    ChangeFeed& changes_;
    cbor::Bytes pcId_;
    std::string pcName_;
    AccountNamer accountName_;
    MonotonicClock clock_;
    WallClock unixSeconds_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::map<std::wstring, Entry> active_;
    std::map<std::wstring, std::deque<ULONGLONG>> creations_;
    bool closed_ = false;
};
