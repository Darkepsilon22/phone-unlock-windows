#pragma once

#include "support/clock.h"
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
    explicit AuthorizationBroker(MonotonicClock clock = SystemMilliseconds);

    void Authorize(const std::wstring& sid);

    std::optional<std::wstring> WaitForAuthorization(const std::vector<std::wstring>& sids,
        const std::function<bool()>& keepWaiting);

private:
    std::optional<std::wstring> TakeMatching(const std::vector<std::wstring>& sids);
    void DropExpired();

    MonotonicClock clock_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::map<std::wstring, ULONGLONG> expirations_;
};
