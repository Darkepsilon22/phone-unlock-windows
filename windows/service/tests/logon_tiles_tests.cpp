#include "ipc/logon_tiles.h"
#include "test_runner.h"

namespace
{
const std::wstring Alice = L"S-1-5-21-1-2-3-1001";
const std::wstring Bob = L"S-1-5-21-1-2-3-1002";

bool StopWaiting()
{
    return false;
}

TEST(ShownTileIsFoundWithItsScenario)
{
    ChangeFeed feed;
    LogonTiles tiles(feed);
    const uint64_t before = feed.Version();
    const std::optional<uint64_t> ticket = tiles.Register(TileScenario::Unlock, { Alice });
    CHECK(ticket.has_value());
    CHECK(feed.Version() > before);
    CHECK(tiles.Find(Alice)->scenario == TileScenario::Unlock);
    CHECK(!tiles.Find(Bob).has_value());
}

TEST(HiddenTileIsForgotten)
{
    ChangeFeed feed;
    LogonTiles tiles(feed);
    const uint64_t ticket = *tiles.Register(TileScenario::Logon, { Alice });
    tiles.Unregister(ticket);
    CHECK(!tiles.Find(Alice).has_value());
}

TEST(EachDisplayGetsANewContext)
{
    ChangeFeed feed;
    LogonTiles tiles(feed);
    const uint64_t first = *tiles.Register(TileScenario::Unlock, { Alice });
    const ContextId firstContext = tiles.Find(Alice)->context;
    tiles.Unregister(first);
    tiles.Register(TileScenario::Unlock, { Alice });
    CHECK(tiles.Find(Alice)->context != firstContext);
}

TEST(AuthorizationReachesOnlyTheMatchingContext)
{
    ChangeFeed feed;
    LogonTiles tiles(feed);
    const uint64_t ticket = *tiles.Register(TileScenario::Unlock, { Alice });
    ContextId stale = tiles.Find(Alice)->context;
    stale[0] ^= 1;
    CHECK(!tiles.Authorize(Alice, stale));
    CHECK(!tiles.WaitForAuthorization(ticket, StopWaiting).has_value());

    CHECK(tiles.Authorize(Alice, tiles.Find(Alice)->context));
    CHECK(tiles.WaitForAuthorization(ticket, StopWaiting) == Alice);
    CHECK(!tiles.WaitForAuthorization(ticket, StopWaiting).has_value());
}

TEST(AuthorizationNeedsTheAccountOnTheTile)
{
    ChangeFeed feed;
    LogonTiles tiles(feed);
    tiles.Register(TileScenario::Unlock, { Alice });
    CHECK(!tiles.Authorize(Bob, tiles.Find(Alice)->context));
}

TEST(AuthorizationAfterTileHiddenIsRefused)
{
    ChangeFeed feed;
    LogonTiles tiles(feed);
    const uint64_t ticket = *tiles.Register(TileScenario::Unlock, { Alice });
    const ContextId context = tiles.Find(Alice)->context;
    tiles.Unregister(ticket);
    CHECK(!tiles.Authorize(Alice, context));
}
}
