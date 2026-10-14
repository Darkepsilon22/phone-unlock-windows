#pragma once

#include "secure_vector.h"
#include <windows.h>
#include <optional>
#include <string>

struct LogonName
{
    std::wstring user;
    std::wstring domain;
};

std::optional<LogonName> LogonNameFor(const std::wstring& sid);

DWORD VerifyAccountPassword(const LogonName& name, const SecureString& password);
