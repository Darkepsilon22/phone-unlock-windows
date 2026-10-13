#pragma once

#include <windows.h>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class AuthorizationBroker
{
public:
    void Authorize(const std::wstring& sid);

    std::optional<std::wstring> WaitForAuthorization(const std::vector<std::wstring>& sids,
        const std::function<bool()>& clientConnected);

    void Stop();

private:
    std::optional<std::wstring> TakeMatching(const std::vector<std::wstring>& sids);
    void DropExpired();

    std::mutex mutex_;
    std::condition_variable changed_;
    std::map<std::wstring, ULONGLONG> expirations_;
    bool stopping_ = false;
};
