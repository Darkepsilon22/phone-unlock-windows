#pragma once

#include "ipc/logon_tiles.h"
#include <map>
#include <optional>
#include <string>

enum class SessionState
{
    Locked,
    LogonScreen,
    SignedIn,
    Unknown,
};

struct AccountView
{
    SessionState state = SessionState::Unknown;
    bool unlockAvailable = false;
};

using SessionLocks = std::map<std::wstring, bool>;

AccountView DescribeAccount(const std::optional<TileView>& tile, const std::optional<SessionLocks>& sessions,
    const std::wstring& sid);

const char* SessionStateText(SessionState state);
