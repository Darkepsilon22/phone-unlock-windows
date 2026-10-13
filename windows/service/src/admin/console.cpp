#include "admin/console.h"
#include <cstdio>

namespace
{
constexpr DWORD MaxSecretLength = 256;
constexpr DWORD AnswerLength = 8;

DWORD WithoutLineEnding(const wchar_t* text, DWORD length)
{
    while (length > 0 && (text[length - 1] == L'\r' || text[length - 1] == L'\n'))
    {
        --length;
    }
    return length;
}
}

std::optional<SecureString> ReadHiddenLine()
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (!GetConsoleMode(input, &mode) || !SetConsoleMode(input, mode & ~ENABLE_ECHO_INPUT))
    {
        return std::nullopt;
    }

    wchar_t buffer[MaxSecretLength + 2] = {};
    DWORD read = 0;
    const BOOL ok = ReadConsoleW(input, buffer, MaxSecretLength + 2, &read, nullptr);
    SetConsoleMode(input, mode);
    fwprintf(stdout, L"\n");

    read = WithoutLineEnding(buffer, read);
    std::optional<SecureString> line;
    if (ok && read > 0 && read <= MaxSecretLength)
    {
        line = SecureString(buffer, read);
    }
    SecureZeroMemory(buffer, sizeof(buffer));
    return line;
}

bool AskYesNo(const wchar_t* question)
{
    fwprintf(stdout, L"%s (o/n) : ", question);
    wchar_t answer[AnswerLength] = {};
    DWORD read = 0;
    ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), answer, AnswerLength, &read, nullptr);
    return read > 0 && (answer[0] == L'o' || answer[0] == L'O');
}
