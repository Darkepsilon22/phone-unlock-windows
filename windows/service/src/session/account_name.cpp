#include "session/account_name.h"
#include "encoding/text.h"
#include <windows.h>
#include <sddl.h>

namespace
{
constexpr size_t MaxAccountNameCharacters = 64;
}

std::string AccountDisplayName(const std::wstring& sid)
{
    PSID binary = nullptr;
    if (!ConvertStringSidToSidW(sid.c_str(), &binary))
    {
        return "compte";
    }
    wchar_t name[256] = {};
    wchar_t domain[256] = {};
    DWORD nameSize = 256;
    DWORD domainSize = 256;
    SID_NAME_USE use = SidTypeUnknown;
    const BOOL found = LookupAccountSidW(nullptr, binary, name, &nameSize, domain, &domainSize, &use);
    LocalFree(binary);
    return found ? SanitizedDisplayName(Utf8FromWide(name), MaxAccountNameCharacters) : "compte";
}
