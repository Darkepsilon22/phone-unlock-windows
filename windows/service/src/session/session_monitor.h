#pragma once

#include "session/pc_state.h"
#include "support/change_feed.h"
#include <windows.h>
#include <map>
#include <mutex>

class SessionMonitor
{
public:
    explicit SessionMonitor(ChangeFeed& changes);

    void OnSessionEvent(DWORD eventType, DWORD sessionId);
    SessionSummary Summary();

private:
    ChangeFeed& changes_;
    std::mutex mutex_;
    bool known_ = false;
    std::map<DWORD, bool> lockedBySession_;
};
