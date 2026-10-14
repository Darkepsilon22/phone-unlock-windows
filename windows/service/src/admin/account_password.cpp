#include "admin/account_password.h"
#include <lm.h>
#include <sddl.h>

namespace
{
constexpr DWORD InternetIdentityLevel = 24;

std::optional<LogonName> LocalName(PSID sid)
{
    wchar_t user[UNLEN + 1] = {};
    wchar_t domain[DNLEN + 1] = {};
    DWORD userSize = UNLEN + 1;
    DWORD domainSize = DNLEN + 1;
    SID_NAME_USE use = SidTypeUnknown;
    if (!LookupAccountSidW(nullptr, sid, user, &userSize, domain, &domainSize, &use) || use != SidTypeUser)
    {
        return std::nullopt;
    }
    return LogonName{ user, domain };
}

std::optional<LogonName> InternetName(const std::wstring& localUser)
{
    USER_INFO_24* info = nullptr;
    if (NetUserGetInfo(nullptr, localUser.c_str(), InternetIdentityLevel, reinterpret_cast<LPBYTE*>(&info)) != NERR_Success)
    {
        return std::nullopt;
    }
    std::optional<LogonName> name;
    if (info->usri24_internet_identity && info->usri24_internet_principal_name && info->usri24_internet_provider_name)
    {
        name = LogonName{ info->usri24_internet_principal_name, info->usri24_internet_provider_name };
    }
    NetApiBufferFree(info);
    return name;
}
}

std::optional<LogonName> LogonNameFor(const std::wstring& sid)
{
    PSID binary = nullptr;
    if (!ConvertStringSidToSidW(sid.c_str(), &binary))
    {
        return std::nullopt;
    }
    std::optional<LogonName> local = LocalName(binary);
    LocalFree(binary);
    if (!local)
    {
        return std::nullopt;
    }
    std::optional<LogonName> internet = InternetName(local->user);
    return internet ? internet : local;
}

DWORD VerifyAccountPassword(const LogonName& name, const SecureString& password)
{
    HANDLE token = nullptr;
    if (!LogonUserW(name.user.c_str(), name.domain.c_str(), password.c_str(), LOGON32_LOGON_INTERACTIVE,
            LOGON32_PROVIDER_DEFAULT, &token))
    {
        return GetLastError();
    }
    CloseHandle(token);
    return NO_ERROR;
}
