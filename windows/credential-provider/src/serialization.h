#pragma once

#include "common.h"

HRESULT SerializeLocalAccountLogon(PCWSTR qualifiedUserName, PCWSTR protectedPassword,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, BYTE** buffer, DWORD* size);

HRESULT SerializeProviderAccountLogon(PCWSTR qualifiedUserName, PCWSTR protectedPassword,
    BYTE** buffer, DWORD* size);

HRESULT GetNegotiatePackageId(ULONG* packageId);
