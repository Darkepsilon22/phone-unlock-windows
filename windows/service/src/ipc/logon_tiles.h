#pragma once

#include "pipe_protocol.h"
#include "support/change_feed.h"
#include <array>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

using ContextId = std::array<uint8_t, 16>;

using pipe_protocol::TileScenario;

struct TileView
{
    ContextId context = {};
    TileScenario scenario = TileScenario::Logon;
};

class LogonTiles
{
public:
    explicit LogonTiles(ChangeFeed& changes);

    std::optional<uint64_t> Register(TileScenario scenario, std::vector<std::wstring> sids);
    void Unregister(uint64_t ticket);
    std::optional<std::wstring> WaitForAuthorization(uint64_t ticket, const std::function<bool()>& keepWaiting);

    std::optional<TileView> Find(const std::wstring& sid);
    bool Authorize(const std::wstring& sid, const ContextId& context);

private:
    struct Tile
    {
        uint64_t ticket = 0;
        TileView view;
        std::vector<std::wstring> sids;
        std::optional<std::wstring> authorized;
    };

    Tile* TileFor(uint64_t ticket);

    ChangeFeed& changes_;
    std::mutex mutex_;
    std::condition_variable authorized_;
    std::vector<Tile> tiles_;
    uint64_t nextTicket_ = 1;
};
