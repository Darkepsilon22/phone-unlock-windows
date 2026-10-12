#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include "log.h"

namespace
{
constexpr size_t MaxLineLength = 640;
constexpr size_t MaxUtf8Length = MaxLineLength * 3;
constexpr wchar_t LogDirectory[] = L"%ProgramData%\\PhoneUnlockProto";
constexpr wchar_t LogFileName[] = L"cp.log";

bool FormatLine(wchar_t (&line)[MaxLineLength], const wchar_t* format, va_list args)
{
    wchar_t message[MaxLineLength];
    if (_vsnwprintf_s(message, _TRUNCATE, format, args) < 0 && message[0] == L'\0')
    {
        return false;
    }

    SYSTEMTIME now;
    GetLocalTime(&now);
    return swprintf_s(line, L"%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu %s\r\n",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        now.wMilliseconds, GetCurrentProcessId(), message) > 0;
}

bool ResolveLogPath(wchar_t (&path)[MAX_PATH])
{
    wchar_t directory[MAX_PATH];
    if (ExpandEnvironmentStringsW(LogDirectory, directory, MAX_PATH) == 0)
    {
        return false;
    }
    CreateDirectoryW(directory, nullptr);
    return swprintf_s(path, L"%s\\%s", directory, LogFileName) > 0;
}

void AppendUtf8(const wchar_t* path, const wchar_t* line)
{
    char utf8[MaxUtf8Length];
    const int size = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (size <= 1)
    {
        return;
    }

    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(file, utf8, static_cast<DWORD>(size - 1), &written, nullptr);
    CloseHandle(file);
}
}

void LogLine(const wchar_t* format, ...)
{
    wchar_t line[MaxLineLength];
    va_list args;
    va_start(args, format);
    const bool formatted = FormatLine(line, format, args);
    va_end(args);

    wchar_t path[MAX_PATH];
    if (formatted && ResolveLogPath(path))
    {
        AppendUtf8(path, line);
    }
}
