#include "session/session_monitor.h"
#include "log.h"
#include <windows.h>
#include <lmcons.h>
#include <wtsapi32.h>
#include <sddl.h>

namespace
{
std::optional<std::wstring> SidOf(const std::wstring& domain, const std::wstring& user)
{
    const std::wstring qualified = domain + L"\\" + user;
    BYTE sid[SECURITY_MAX_SID_SIZE] = {};
    DWORD sidSize = sizeof(sid);
    wchar_t referencedDomain[DNLEN + 1] = {};
    DWORD domainSize = DNLEN + 1;
    SID_NAME_USE use = SidTypeUnknown;
    LPWSTR text = nullptr;
    if (!LookupAccountNameW(nullptr, qualified.c_str(), sid, &sidSize, referencedDomain, &domainSize, &use) ||
        !ConvertSidToStringSidW(sid, &text))
    {
        return std::nullopt;
    }
    std::wstring result(text);
    LocalFree(text);
    return result;
}

std::optional<std::pair<std::wstring, bool>> DescribeSession(DWORD sessionId)
{
    LPWSTR buffer = nullptr;
    DWORD size = 0;
    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionId, WTSSessionInfoEx, &buffer, &size))
    {
        LogLine(L"session %lu query failed error=%lu", sessionId, GetLastError());
        return std::nullopt;
    }
    const auto* info = reinterpret_cast<const WTSINFOEXW*>(buffer);
    std::optional<std::pair<std::wstring, bool>> result;
    if (info->Level == 1 && info->Data.WTSInfoExLevel1.UserName[0] != L'\0')
    {
        const WTSINFOEX_LEVEL1_W& session = info->Data.WTSInfoExLevel1;
        if (std::optional<std::wstring> sid = SidOf(session.DomainName, session.UserName))
        {
            result.emplace(std::move(*sid), session.SessionFlags == WTS_SESSIONSTATE_LOCK);
        }
    }
    WTSFreeMemory(buffer);
    return result;
}

std::optional<SessionLocks> QuerySessions()
{
    WTS_SESSION_INFOW* sessions = nullptr;
    DWORD count = 0;
    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count))
    {
        LogLine(L"session enumeration failed error=%lu", GetLastError());
        return std::nullopt;
    }
    SessionLocks locks;
    for (DWORD index = 0; index < count; ++index)
    {
        if (std::optional<std::pair<std::wstring, bool>> session = DescribeSession(sessions[index].SessionId))
        {
            const auto [entry, inserted] = locks.try_emplace(session->first, session->second);
            entry->second = entry->second && session->second;
        }
    }
    WTSFreeMemory(sessions);
    return locks;
}
}

SessionMonitor::SessionMonitor(ChangeFeed& changes) : changes_(changes)
{
}

void SessionMonitor::Refresh()
{
    std::optional<SessionLocks> current = QuerySessions();
    {
        std::lock_guard lock(mutex_);
        if (current == sessions_)
        {
            return;
        }
        sessions_ = std::move(current);
    }
    LogLine(L"sessions changed");
    changes_.Notify();
}

std::optional<SessionLocks> SessionMonitor::Snapshot()
{
    std::lock_guard lock(mutex_);
    return sessions_;
}
