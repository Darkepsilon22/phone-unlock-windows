#include "support/change_feed.h"

void ChangeFeed::Notify()
{
    {
        std::lock_guard lock(mutex_);
        ++version_;
    }
    changed_.notify_all();
}

void ChangeFeed::Close()
{
    {
        std::lock_guard lock(mutex_);
        closed_ = true;
    }
    changed_.notify_all();
}

uint64_t ChangeFeed::Version()
{
    std::lock_guard lock(mutex_);
    return version_;
}

uint64_t ChangeFeed::WaitForChange(uint64_t since, std::chrono::milliseconds timeout)
{
    std::unique_lock lock(mutex_);
    changed_.wait_for(lock, timeout, [this, since] { return closed_ || version_ != since; });
    return version_;
}
