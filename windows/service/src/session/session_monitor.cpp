#include "session/session_monitor.h"
#include "log.h"

SessionMonitor::SessionMonitor(ChangeFeed& changes) : changes_(changes)
{
}

void SessionMonitor::OnSessionEvent(DWORD eventType, DWORD sessionId)
{
    {
        std::lock_guard lock(mutex_);
        switch (eventType)
        {
        case WTS_SESSION_LOGON:
        case WTS_SESSION_UNLOCK:
            lockedBySession_[sessionId] = false;
            break;
        case WTS_SESSION_LOCK:
            lockedBySession_[sessionId] = true;
            break;
        case WTS_SESSION_LOGOFF:
            lockedBySession_.erase(sessionId);
            break;
        default:
            return;
        }
        known_ = true;
    }
    LogLine(L"session event type=%lu session=%lu", eventType, sessionId);
    changes_.Notify();
}

SessionSummary SessionMonitor::Summary()
{
    std::lock_guard lock(mutex_);
    SessionSummary summary;
    summary.known = known_;
    for (const auto& [session, locked] : lockedBySession_)
    {
        summary.anyLocked = summary.anyLocked || locked;
        summary.anyUnlocked = summary.anyUnlocked || !locked;
    }
    return summary;
}
