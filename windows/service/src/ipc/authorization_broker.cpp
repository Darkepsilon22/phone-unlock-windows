#include "ipc/authorization_broker.h"
#include "log.h"
#include <algorithm>
#include <chrono>

namespace
{
constexpr ULONGLONG AuthorizationLifetime = 30'000;
constexpr auto ConnectionCheckInterval = std::chrono::seconds(1);
}

void AuthorizationBroker::Authorize(const std::wstring& sid)
{
    {
        std::lock_guard lock(mutex_);
        expirations_[sid] = GetTickCount64() + AuthorizationLifetime;
    }
    LogLine(L"authorization created");
    changed_.notify_all();
}

std::optional<std::wstring> AuthorizationBroker::WaitForAuthorization(const std::vector<std::wstring>& sids,
    const std::function<bool()>& clientConnected)
{
    std::unique_lock lock(mutex_);
    for (;;)
    {
        DropExpired();
        if (stopping_)
        {
            return std::nullopt;
        }
        if (auto sid = TakeMatching(sids))
        {
            return sid;
        }
        lock.unlock();
        const bool connected = clientConnected();
        lock.lock();
        if (!connected)
        {
            return std::nullopt;
        }
        changed_.wait_for(lock, ConnectionCheckInterval);
    }
}

void AuthorizationBroker::Stop()
{
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    changed_.notify_all();
}

std::optional<std::wstring> AuthorizationBroker::TakeMatching(const std::vector<std::wstring>& sids)
{
    const auto match = std::find_if(sids.begin(), sids.end(),
        [this](const std::wstring& sid) { return expirations_.contains(sid); });
    if (match == sids.end())
    {
        return std::nullopt;
    }
    expirations_.erase(*match);
    LogLine(L"authorization consumed");
    return *match;
}

void AuthorizationBroker::DropExpired()
{
    const ULONGLONG now = GetTickCount64();
    std::erase_if(expirations_, [now](const auto& entry) { return entry.second <= now; });
}
