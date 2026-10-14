#pragma once

#include "session/pc_state.h"
#include "support/change_feed.h"
#include <mutex>
#include <optional>

class SessionMonitor
{
public:
    explicit SessionMonitor(ChangeFeed& changes);

    void Refresh();
    std::optional<SessionLocks> Snapshot();

private:
    ChangeFeed& changes_;
    std::mutex mutex_;
    std::optional<SessionLocks> sessions_;
};
