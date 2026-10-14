#include "session/pc_state.h"
#include "test_runner.h"

namespace
{
const std::wstring Alice = L"S-1-5-21-1-2-3-1001";

TileView Tile(TileScenario scenario)
{
    TileView tile;
    tile.scenario = scenario;
    return tile;
}

TEST(LockScreenTileMeansLockedAndUnlockable)
{
    const AccountView view = DescribeAccount(Tile(TileScenario::Unlock), SessionLocks{}, Alice);
    CHECK(view.state == SessionState::Locked);
    CHECK(view.unlockAvailable);
}

TEST(LogonTileMeansLogonScreenAndUnlockable)
{
    const AccountView view = DescribeAccount(Tile(TileScenario::Logon), std::nullopt, Alice);
    CHECK(view.state == SessionState::LogonScreen);
    CHECK(view.unlockAvailable);
}

TEST(OpenSessionWithoutTileIsSignedIn)
{
    const AccountView view = DescribeAccount(std::nullopt, SessionLocks{ { Alice, false } }, Alice);
    CHECK(view.state == SessionState::SignedIn);
    CHECK(!view.unlockAvailable);
}

TEST(LockedSessionWithoutTileCannotBeUnlocked)
{
    const AccountView view = DescribeAccount(std::nullopt, SessionLocks{ { Alice, true } }, Alice);
    CHECK(view.state == SessionState::Locked);
    CHECK(!view.unlockAvailable);
}

TEST(NoSessionMeansLogonScreen)
{
    const AccountView view = DescribeAccount(std::nullopt, SessionLocks{}, Alice);
    CHECK(view.state == SessionState::LogonScreen);
    CHECK(!view.unlockAvailable);
}

TEST(UnreadableSessionsMeanUnknown)
{
    const AccountView view = DescribeAccount(std::nullopt, std::nullopt, Alice);
    CHECK(view.state == SessionState::Unknown);
    CHECK(!view.unlockAvailable);
    CHECK(std::string(SessionStateText(view.state)) == "unknown");
}
}
