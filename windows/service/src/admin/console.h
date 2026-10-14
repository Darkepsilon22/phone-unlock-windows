#pragma once

#include "secure_vector.h"
#include <optional>
#include <string>

std::optional<SecureString> ReadHiddenLine();

bool AskYesNo(const wchar_t* question);

std::wstring LocalDateTime(const std::wstring& unixSeconds);
