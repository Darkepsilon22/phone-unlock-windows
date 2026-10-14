#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

class ChangeFeed
{
public:
    void Notify();
    uint64_t Version();
    uint64_t WaitForChange(uint64_t since, std::chrono::milliseconds timeout);

private:
    std::mutex mutex_;
    std::condition_variable changed_;
    uint64_t version_ = 1;
};
