#pragma once

#include <windows.h>
#include <functional>

using MonotonicClock = std::function<ULONGLONG()>;

inline ULONGLONG SystemMilliseconds()
{
    return GetTickCount64();
}
