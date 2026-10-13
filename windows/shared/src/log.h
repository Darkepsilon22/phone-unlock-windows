#pragma once

#include <windows.h>

void ConfigureLog(PCWSTR directory, PCWSTR fileName);

void LogLine(_Printf_format_string_ const wchar_t* format, ...);
