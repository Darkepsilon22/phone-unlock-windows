#include "session/pc_state.h"
#include "session/session_monitor.h"
#include "test_runner.h"

namespace
{
TileView Tile(TileScenario scenario)
{
    TileView tile;
    tile.scenario = scenario;
    return tile;
}

SessionSummary Known(bool anyUnlocked, bool anyLocked)
{
    return { true, anyUnlocked, anyLocked };
}

TEST(LockScreenTileMeansLockedAndUnlockable)
{
    const AccountView view = DescribeAccount(Tile(TileScenario::Unlock), Known(false, true));
    CHECK(view.state == SessionState::Locked);
    CHECK(view.unlockAvailable);
}

TEST(LogonTileMeansLogonScreenAndUnlockable)
{
    const AccountView view = DescribeAccount(Tile(TileScenario::Logon), SessionSummary{});
    CHECK(view.state == SessionState::LogonScreen);
    CHECK(view.unlockAvailable);
}

TEST(LogonTileOverALockedSessionMeansLocked)
{
    const AccountView view = DescribeAccount(Tile(TileScenario::Logon), Known(false, true));
    CHECK(view.state == SessionState::Locked);
    CHECK(view.unlockAvailable);
}

TEST(OpenSessionWithoutTileIsSignedIn)
{
    const AccountView view = DescribeAccount(std::nullopt, Known(true, false));
    CHECK(view.state == SessionState::SignedIn);
    CHECK(!view.unlockAvailable);
}

TEST(LockedSessionWithoutTileCannotBeUnlocked)
{
    const AccountView view = DescribeAccount(std::nullopt, Known(false, true));
    CHECK(view.state == SessionState::Locked);
    CHECK(!view.unlockAvailable);
}

TEST(NoSessionMeansLogonScreen)
{
    const AccountView view = DescribeAccount(std::nullopt, Known(false, false));
    CHECK(view.state == SessionState::LogonScreen);
    CHECK(!view.unlockAvailable);
}

TEST(NoEventYetMeansUnknown)
{
    const AccountView view = DescribeAccount(std::nullopt, SessionSummary{});
    CHECK(view.state == SessionState::Unknown);
    CHECK(!view.unlockAvailable);
    CHECK(std::string(SessionStateText(view.state)) == "unknown");
}

TEST(SessionEventsTrackLockAndLogoff)
{
    ChangeFeed feed;
    SessionMonitor monitor(feed);
    CHECK(!monitor.Summary().known);

    const uint64_t before = feed.Version();
    monitor.OnSessionEvent(WTS_SESSION_LOCK, 1);
    CHECK(feed.Version() > before);
    CHECK(monitor.Summary().known);
    CHECK(monitor.Summary().anyLocked);
    CHECK(!monitor.Summary().anyUnlocked);

    monitor.OnSessionEvent(WTS_SESSION_UNLOCK, 1);
    CHECK(monitor.Summary().anyUnlocked);
    CHECK(!monitor.Summary().anyLocked);

    monitor.OnSessionEvent(WTS_SESSION_LOGOFF, 1);
    CHECK(!monitor.Summary().anyUnlocked);
    CHECK(!monitor.Summary().anyLocked);
}

TEST(UnrelatedSessionEventsAreIgnored)
{
    ChangeFeed feed;
    SessionMonitor monitor(feed);
    const uint64_t before = feed.Version();
    monitor.OnSessionEvent(WTS_CONSOLE_CONNECT, 1);
    CHECK(feed.Version() == before);
    CHECK(!monitor.Summary().known);
}
}
