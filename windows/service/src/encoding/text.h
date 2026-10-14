#pragma once

#include <cstdint>
#include <span>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

std::string Utf8FromWide(const std::wstring& text);

std::wstring WideFromUtf8(const std::string& text);

std::wstring HexFromBytes(std::span<const uint8_t> bytes);

std::optional<std::vector<uint8_t>> BytesFromHex(std::wstring_view hex);

std::string Base64Url(std::span<const uint8_t> bytes);

std::string SanitizedDisplayName(const std::string& utf8, size_t maxCharacters);
