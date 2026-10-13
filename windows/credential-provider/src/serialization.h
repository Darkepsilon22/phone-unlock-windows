#pragma once

#include "common.h"

enum class SerializationFormat : DWORD
{
    ProviderCredentials = 0,
    InteractiveLogon = 1,
    ProtectedCredentials = 2,
};

HRESULT SerializeLogon(SerializationFormat format, PCWSTR qualifiedUserName, PCWSTR protectedPassword,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, BYTE** buffer, DWORD* size);

HRESULT GetNegotiatePackageId(ULONG* packageId);
