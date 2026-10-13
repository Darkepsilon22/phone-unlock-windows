#include "encoding/text.h"
#include <windows.h>

std::string Utf8FromWide(const std::wstring& text)
{
    if (text.empty())
    {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
        nullptr, nullptr);
    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

std::wstring WideFromUtf8(const std::string& text)
{
    if (text.empty())
    {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
        nullptr, 0);
    if (size <= 0)
    {
        return {};
    }
    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

std::wstring HexFromBytes(std::span<const uint8_t> bytes)
{
    static constexpr wchar_t Digits[] = L"0123456789abcdef";
    std::wstring hex;
    hex.reserve(bytes.size() * 2);
    for (uint8_t byte : bytes)
    {
        hex.push_back(Digits[byte >> 4]);
        hex.push_back(Digits[byte & 0x0F]);
    }
    return hex;
}

std::string Base64Url(std::span<const uint8_t> bytes)
{
    static constexpr char Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    size_t index = 0;
    for (; index + 3 <= bytes.size(); index += 3)
    {
        const uint32_t block = (bytes[index] << 16) | (bytes[index + 1] << 8) | bytes[index + 2];
        out.push_back(Alphabet[(block >> 18) & 0x3F]);
        out.push_back(Alphabet[(block >> 12) & 0x3F]);
        out.push_back(Alphabet[(block >> 6) & 0x3F]);
        out.push_back(Alphabet[block & 0x3F]);
    }
    const size_t remaining = bytes.size() - index;
    if (remaining > 0)
    {
        const uint32_t block = (bytes[index] << 16) | (remaining == 2 ? bytes[index + 1] << 8 : 0);
        out.push_back(Alphabet[(block >> 18) & 0x3F]);
        out.push_back(Alphabet[(block >> 12) & 0x3F]);
        if (remaining == 2)
        {
            out.push_back(Alphabet[(block >> 6) & 0x3F]);
        }
    }
    return out;
}

std::string SanitizedDisplayName(const std::string& utf8, size_t maxCharacters)
{
    std::wstring wide = WideFromUtf8(utf8);
    std::wstring clean;
    for (wchar_t character : wide)
    {
        if (character >= L' ' && character != 0x7F && clean.size() < maxCharacters)
        {
            clean.push_back(character);
        }
    }
    return Utf8FromWide(clean);
}
