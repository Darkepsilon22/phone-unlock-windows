#pragma once

#include "ipc/logon_tiles.h"
#include <optional>

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

struct SessionSummary
{
    bool known = false;
    bool anyUnlocked = false;
    bool anyLocked = false;
};

AccountView DescribeAccount(const std::optional<TileView>& tile, const SessionSummary& sessions);

const char* SessionStateText(SessionState state);
