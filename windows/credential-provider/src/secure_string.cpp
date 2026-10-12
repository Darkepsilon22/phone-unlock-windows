#include "secure_string.h"

namespace
{
bool IsAlreadyProtected(PCWSTR password)
{
    CRED_PROTECTION_TYPE protectionType;
    return CredIsProtectedW(const_cast<PWSTR>(password), &protectionType) &&
        protectionType != CredUnprotected;
}

bool MustStayPlain(PCWSTR password, CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario)
{
    return scenario == CPUS_CREDUI || *password == L'\0' || IsAlreadyProtected(password);
}

HRESULT ProtectCopy(PWSTR plainCopy, PWSTR* protectedPassword)
{
    const DWORD plainLength = static_cast<DWORD>(wcslen(plainCopy) + 1);
    DWORD protectedLength = 0;
    if (CredProtectW(FALSE, plainCopy, plainLength, nullptr, &protectedLength, nullptr))
    {
        return E_UNEXPECTED;
    }
    const DWORD error = GetLastError();
    if (error != ERROR_INSUFFICIENT_BUFFER || protectedLength == 0)
    {
        return HRESULT_FROM_WIN32(error);
    }

    auto* buffer = static_cast<PWSTR>(CoTaskMemAlloc(protectedLength * sizeof(wchar_t)));
    if (!buffer)
    {
        return E_OUTOFMEMORY;
    }
    if (!CredProtectW(FALSE, plainCopy, plainLength, buffer, &protectedLength, nullptr))
    {
        const HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        CoTaskMemFree(buffer);
        return hr;
    }
    *protectedPassword = buffer;
    return S_OK;
}
}

void SecureFree(PWSTR* text)
{
    if (*text)
    {
        SecureZeroMemory(*text, wcslen(*text) * sizeof(wchar_t));
        CoTaskMemFree(*text);
        *text = nullptr;
    }
}

HRESULT CopyProtectedPassword(PCWSTR password, CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario,
    PWSTR* protectedPassword)
{
    *protectedPassword = nullptr;
    if (MustStayPlain(password, scenario))
    {
        return SHStrDupW(password, protectedPassword);
    }

    PWSTR plainCopy = nullptr;
    HRESULT hr = SHStrDupW(password, &plainCopy);
    if (SUCCEEDED(hr))
    {
        hr = ProtectCopy(plainCopy, protectedPassword);
    }
    SecureFree(&plainCopy);
    return hr;
}
