#include "session/pc_state.h"

AccountView DescribeAccount(const std::optional<TileView>& tile, const std::optional<SessionLocks>& sessions,
    const std::wstring& sid)
{
    if (tile)
    {
        return { tile->scenario == TileScenario::Unlock ? SessionState::Locked : SessionState::LogonScreen, true };
    }
    if (!sessions)
    {
        return { SessionState::Unknown, false };
    }
    const auto session = sessions->find(sid);
    if (session == sessions->end())
    {
        return { SessionState::LogonScreen, false };
    }
    return { session->second ? SessionState::Locked : SessionState::SignedIn, false };
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
