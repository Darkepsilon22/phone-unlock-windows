#pragma once

#include "common.h"

HRESULT SerializeInteractiveLogon(PCWSTR qualifiedUserName, PCWSTR protectedPassword,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, BYTE** buffer, DWORD* size);

HRESULT GetNegotiatePackageId(ULONG* packageId);
