#pragma once

#include "secure_vector.h"
#include <optional>

std::optional<SecureString> ReadHiddenLine();

bool AskYesNo(const wchar_t* question);
