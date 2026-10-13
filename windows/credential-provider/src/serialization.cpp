#include "serialization.h"

namespace
{
constexpr wchar_t DomainSeparator = L'\\';
constexpr char NegotiatePackageName[] = "Negotiate";

struct QualifiedName
{
    PWSTR domain = nullptr;
    PWSTR user = nullptr;

    ~QualifiedName()
    {
        CoTaskMemFree(domain);
        CoTaskMemFree(user);
    }
};

HRESULT SplitQualifiedUserName(PCWSTR qualifiedUserName, QualifiedName* name)
{
    PCWSTR separator = wcschr(qualifiedUserName, DomainSeparator);
    if (!separator || separator == qualifiedUserName || separator[1] == L'\0')
    {
        return E_UNEXPECTED;
    }

    const size_t domainLength = static_cast<size_t>(separator - qualifiedUserName);
    name->domain = static_cast<PWSTR>(CoTaskMemAlloc((domainLength + 1) * sizeof(wchar_t)));
    if (!name->domain)
    {
        return E_OUTOFMEMORY;
    }
    HRESULT hr = StringCchCopyNW(name->domain, domainLength + 1, qualifiedUserName, domainLength);
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(separator + 1, &name->user);
    }
    return hr;
}

HRESULT LogonMessageType(CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, KERB_LOGON_SUBMIT_TYPE* type)
{
    switch (scenario)
    {
    case CPUS_LOGON:
        *type = KerbInteractiveLogon;
        return S_OK;
    case CPUS_UNLOCK_WORKSTATION:
        *type = KerbWorkstationUnlockLogon;
        return S_OK;
    default:
        return E_INVALIDARG;
    }
}

HRESULT ToUnicodeString(PCWSTR text, UNICODE_STRING* result)
{
    const size_t byteLength = wcslen(text) * sizeof(wchar_t);
    if (byteLength > USHRT_MAX)
    {
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }
    result->Length = static_cast<USHORT>(byteLength);
    result->MaximumLength = result->Length;
    result->Buffer = const_cast<PWSTR>(text);
    return S_OK;
}

void PackUnicodeString(const UNICODE_STRING& source, BYTE* base, BYTE** cursor, UNICODE_STRING* packed)
{
    packed->Length = source.Length;
    packed->MaximumLength = source.Length;
    CopyMemory(*cursor, source.Buffer, source.Length);
    packed->Buffer = reinterpret_cast<PWSTR>(*cursor - base);
    *cursor += source.Length;
}

HRESULT BuildInteractiveLogon(const QualifiedName& name, PCWSTR protectedPassword,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, KERB_INTERACTIVE_LOGON* logon)
{
    HRESULT hr = LogonMessageType(scenario, &logon->MessageType);
    if (SUCCEEDED(hr))
    {
        hr = ToUnicodeString(name.domain, &logon->LogonDomainName);
    }
    if (SUCCEEDED(hr))
    {
        hr = ToUnicodeString(name.user, &logon->UserName);
    }
    if (SUCCEEDED(hr))
    {
        hr = ToUnicodeString(protectedPassword, &logon->Password);
    }
    return hr;
}

HRESULT PackInteractiveUnlockLogon(const KERB_INTERACTIVE_LOGON& logon, BYTE** buffer, DWORD* size)
{
    const DWORD packedSize = sizeof(KERB_INTERACTIVE_UNLOCK_LOGON) +
        logon.LogonDomainName.Length + logon.UserName.Length + logon.Password.Length;
    auto* packed = static_cast<KERB_INTERACTIVE_UNLOCK_LOGON*>(CoTaskMemAlloc(packedSize));
    if (!packed)
    {
        return E_OUTOFMEMORY;
    }
    ZeroMemory(packed, sizeof(*packed));

    BYTE* base = reinterpret_cast<BYTE*>(packed);
    BYTE* cursor = base + sizeof(*packed);
    packed->Logon.MessageType = logon.MessageType;
    PackUnicodeString(logon.LogonDomainName, base, &cursor, &packed->Logon.LogonDomainName);
    PackUnicodeString(logon.UserName, base, &cursor, &packed->Logon.UserName);
    PackUnicodeString(logon.Password, base, &cursor, &packed->Logon.Password);

    *buffer = base;
    *size = packedSize;
    return S_OK;
}
}

HRESULT SerializeInteractiveLogon(PCWSTR qualifiedUserName, PCWSTR protectedPassword,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, BYTE** buffer, DWORD* size)
{
    *buffer = nullptr;
    *size = 0;

    QualifiedName name;
    HRESULT hr = SplitQualifiedUserName(qualifiedUserName, &name);
    KERB_INTERACTIVE_LOGON logon = {};
    if (SUCCEEDED(hr))
    {
        hr = BuildInteractiveLogon(name, protectedPassword, scenario, &logon);
    }
    if (SUCCEEDED(hr))
    {
        hr = PackInteractiveUnlockLogon(logon, buffer, size);
    }
    return hr;
}

HRESULT GetNegotiatePackageId(ULONG* packageId)
{
    *packageId = 0;

    HANDLE lsa = nullptr;
    NTSTATUS status = LsaConnectUntrusted(&lsa);
    if (status < 0)
    {
        return HRESULT_FROM_NT(status);
    }

    char packageName[sizeof(NegotiatePackageName)];
    CopyMemory(packageName, NegotiatePackageName, sizeof(packageName));
    LSA_STRING lsaPackageName;
    lsaPackageName.Buffer = packageName;
    lsaPackageName.Length = static_cast<USHORT>(sizeof(packageName) - 1);
    lsaPackageName.MaximumLength = static_cast<USHORT>(sizeof(packageName));

    status = LsaLookupAuthenticationPackage(lsa, &lsaPackageName, packageId);
    LsaDeregisterLogonProcess(lsa);
    return status < 0 ? HRESULT_FROM_NT(status) : S_OK;
}
