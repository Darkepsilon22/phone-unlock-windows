#include "session/pc_state.h"

AccountView DescribeAccount(const std::optional<TileView>& tile, const SessionSummary& sessions)
{
    if (tile)
    {
        return { tile->scenario == TileScenario::Unlock ? SessionState::Locked : SessionState::LogonScreen, true };
    }
    if (!sessions.known)
    {
        return { SessionState::Unknown, false };
    }
    if (sessions.anyUnlocked)
    {
        return { SessionState::SignedIn, false };
    }
    return { sessions.anyLocked ? SessionState::Locked : SessionState::LogonScreen, false };
}

const char* SessionStateText(SessionState state)
{
    switch (state)
    {
    case SessionState::Locked:
        return "locked";
    case SessionState::LogonScreen:
        return "logon_screen";
    case SessionState::SignedIn:
        return "signed_in";
    default:
        return "unknown";
    }
}
