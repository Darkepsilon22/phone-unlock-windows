#include "ipc/logon_tiles.h"
#include "log.h"
#include <algorithm>
#include <chrono>

namespace
{
constexpr auto KeepWaitingCheckInterval = std::chrono::seconds(1);

bool Lists(const std::vector<std::wstring>& sids, const std::wstring& sid)
{
    return std::find(sids.begin(), sids.end(), sid) != sids.end();
}
}

LogonTiles::LogonTiles(ChangeFeed& changes) : changes_(changes)
{
}

uint64_t LogonTiles::Register(TileScenario scenario, std::vector<std::wstring> sids, const ContextId& context)
{
    Tile tile;
    tile.view.context = context;
    tile.view.scenario = scenario;
    tile.sids = std::move(sids);
    uint64_t ticket = 0;
    {
        std::lock_guard lock(mutex_);
        ticket = nextTicket_++;
        tile.ticket = ticket;
        tiles_.push_back(std::move(tile));
    }
    LogLine(L"tile shown scenario=%d", static_cast<int>(scenario));
    changes_.Notify();
    return ticket;
}

void LogonTiles::Unregister(uint64_t ticket)
{
    {
        std::lock_guard lock(mutex_);
        std::erase_if(tiles_, [ticket](const Tile& tile) { return tile.ticket == ticket; });
    }
    LogLine(L"tile hidden");
    changes_.Notify();
}

std::optional<std::wstring> LogonTiles::WaitForAuthorization(uint64_t ticket, const std::function<bool()>& keepWaiting)
{
    std::unique_lock lock(mutex_);
    for (;;)
    {
        Tile* tile = TileFor(ticket);
        if (!tile)
        {
            return std::nullopt;
        }
        if (tile->authorized)
        {
            std::optional<std::wstring> sid;
            sid.swap(tile->authorized);
            LogLine(L"authorization consumed");
            return sid;
        }
        lock.unlock();
        const bool waiting = keepWaiting();
        lock.lock();
        if (!waiting)
        {
            return std::nullopt;
        }
        authorized_.wait_for(lock, KeepWaitingCheckInterval);
    }
}

std::optional<TileView> LogonTiles::Find(const std::wstring& sid)
{
    std::lock_guard lock(mutex_);
    const auto match = std::find_if(tiles_.rbegin(), tiles_.rend(), [&sid](const Tile& tile) { return Lists(tile.sids, sid); });
    return match == tiles_.rend() ? std::nullopt : std::optional(match->view);
}

bool LogonTiles::Authorize(const std::wstring& sid, const ContextId& context)
{
    {
        std::lock_guard lock(mutex_);
        const auto match = std::find_if(tiles_.begin(), tiles_.end(),
            [&](const Tile& tile) { return tile.view.context == context && Lists(tile.sids, sid); });
        if (match == tiles_.end())
        {
            return false;
        }
        match->authorized = sid;
    }
    LogLine(L"authorization created");
    authorized_.notify_all();
    return true;
}

LogonTiles::Tile* LogonTiles::TileFor(uint64_t ticket)
{
    const auto match = std::find_if(tiles_.begin(), tiles_.end(), [ticket](const Tile& tile) { return tile.ticket == ticket; });
    return match == tiles_.end() ? nullptr : &*match;
}
