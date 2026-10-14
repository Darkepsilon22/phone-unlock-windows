#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include "log.h"

namespace
{
constexpr size_t MaxLineLength = 640;
constexpr size_t MaxUtf8Length = MaxLineLength * 3;

wchar_t logPath[MAX_PATH] = {};

bool FormatLine(wchar_t (&line)[MaxLineLength], const wchar_t* format, va_list args)
{
    wchar_t message[MaxLineLength];
    if (_vsnwprintf_s(message, _TRUNCATE, format, args) < 0 && message[0] == L'\0')
    {
        return false;
    }

    SYSTEMTIME now;
    GetLocalTime(&now);
    return swprintf_s(line, L"%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu tid=%lu %s\r\n",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        now.wMilliseconds, GetCurrentProcessId(), GetCurrentThreadId(), message) > 0;
}

void AppendUtf8(const wchar_t* line)
{
    char utf8[MaxUtf8Length];
    const int size = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (size <= 1)
    {
        return;
    }

    HANDLE file = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
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

void ConfigureLog(PCWSTR directory, PCWSTR fileName)
{
    wchar_t logDirectory[MAX_PATH] = {};
    if (ExpandEnvironmentStringsW(directory, logDirectory, MAX_PATH) == 0 ||
        swprintf_s(logPath, L"%s\\%s", logDirectory, fileName) < 0)
    {
        logPath[0] = L'\0';
    }
}

void LogLine(const wchar_t* format, ...)
{
    if (logPath[0] == L'\0')
    {
        return;
    }

    wchar_t line[MaxLineLength];
    va_list args;
    va_start(args, format);
    const bool formatted = FormatLine(line, format, args);
    va_end(args);

    if (formatted)
    {
        AppendUtf8(line);
    }
}
