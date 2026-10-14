#pragma once

#include "secure_vector.h"
#include <optional>
#include <string>

struct ConfirmedAccount
{
    std::wstring sid;
    SecureString password;
};

std::optional<ConfirmedAccount> ConfirmAccountPassword();
