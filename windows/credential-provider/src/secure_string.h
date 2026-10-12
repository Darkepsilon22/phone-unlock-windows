#pragma once

#include "common.h"

void SecureFree(PWSTR* text);

HRESULT CopyProtectedPassword(PCWSTR password, CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario,
    PWSTR* protectedPassword);
