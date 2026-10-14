#pragma once

#include <windows.h>
#include <cstdint>
#include <ctime>
#include <functional>

using MonotonicClock = std::function<ULONGLONG()>;
using WallClock = std::function<uint64_t()>;

inline ULONGLONG SystemMilliseconds()
{
    return GetTickCount64();
}

inline uint64_t SystemUnixSeconds()
{
    return static_cast<uint64_t>(std::time(nullptr));
}
