#pragma once

#include "support/clock.h"
#include <mutex>
#include <optional>
#include <string>

struct UnlockOutcome
{
    uint64_t unixSeconds = 0;
    std::string phoneName;
    LONG status = 0;
};

struct RecordedError
{
    uint64_t unixSeconds = 0;
    std::string phoneName;
    std::string code;
};

class Diagnostics
{
public:
    explicit Diagnostics(WallClock unixSeconds = SystemUnixSeconds);

    void RecordUnlock(const std::string& phoneName, LONG status);
    void RecordError(const std::string& phoneName, const std::string& code);
    std::optional<UnlockOutcome> LastUnlock() const;
    std::optional<RecordedError> LastError() const;

private:
    WallClock unixSeconds_;
    mutable std::mutex mutex_;
    std::optional<UnlockOutcome> lastUnlock_;
    std::optional<RecordedError> lastError_;
};
