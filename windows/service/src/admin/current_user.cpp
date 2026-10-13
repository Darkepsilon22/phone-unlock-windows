#include "admin/current_user.h"
#include <windows.h>
#include <sddl.h>
#include <cstdio>

std::optional<std::wstring> CurrentUserSid()
{
    HANDLE token = nullptr;
    BYTE buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    DWORD size = 0;
    const BOOL read = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) &&
        GetTokenInformation(token, TokenUser, buffer, sizeof(buffer), &size);
    if (token)
    {
        CloseHandle(token);
    }

    LPWSTR text = nullptr;
    if (!read || !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer)->User.Sid, &text))
    {
        fwprintf(stderr, L"SID du compte introuvable.\n");
        return std::nullopt;
    }
    std::wstring sid(text);
    LocalFree(text);
    return sid;
}
