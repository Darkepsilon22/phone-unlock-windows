#include "settings.h"

namespace
{
constexpr wchar_t SettingsKey[] = L"SOFTWARE\\PhoneUnlockProto";
constexpr wchar_t SerializationFormatValue[] = L"SerializationFormat";
constexpr DWORD LastFormat = static_cast<DWORD>(SerializationFormat::ProtectedCredentials);

bool ReadDword(PCWSTR name, DWORD* value)
{
    DWORD size = sizeof(*value);
    return RegGetValueW(HKEY_LOCAL_MACHINE, SettingsKey, name, RRF_RT_REG_DWORD, nullptr, value, &size) ==
        ERROR_SUCCESS;
}

constexpr SerializationFormat DefaultFormat = SerializationFormat::InteractiveLogon;
}

SerializationFormat ConfiguredSerializationFormat()
{
    DWORD configured = 0;
    if (ReadDword(SerializationFormatValue, &configured) && configured <= LastFormat)
    {
        return static_cast<SerializationFormat>(configured);
    }
    return DefaultFormat;
}
