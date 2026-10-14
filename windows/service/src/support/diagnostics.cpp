#include "support/diagnostics.h"

namespace
{
constexpr char WindowsRejected[] = "WINDOWS_REJECTED";
}

Diagnostics::Diagnostics(WallClock unixSeconds) : unixSeconds_(std::move(unixSeconds))
{
}

void Diagnostics::RecordUnlock(const std::string& phoneName, LONG status)
{
    const uint64_t now = unixSeconds_();
    std::lock_guard lock(mutex_);
    lastUnlock_ = UnlockOutcome{ now, phoneName, status };
    if (status != 0)
    {
        lastError_ = RecordedError{ now, phoneName, WindowsRejected };
    }
}

void Diagnostics::RecordError(const std::string& phoneName, const std::string& code)
{
    const uint64_t now = unixSeconds_();
    std::lock_guard lock(mutex_);
    lastError_ = RecordedError{ now, phoneName, code };
}

std::optional<UnlockOutcome> Diagnostics::LastUnlock() const
{
    std::lock_guard lock(mutex_);
    return lastUnlock_;
}

std::optional<RecordedError> Diagnostics::LastError() const
{
    std::lock_guard lock(mutex_);
    return lastError_;
}

void Diagnostics::Clear()
{
    std::lock_guard lock(mutex_);
    lastUnlock_.reset();
    lastError_.reset();
}
