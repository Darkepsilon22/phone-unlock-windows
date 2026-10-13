#pragma once

#include <windows.h>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

HRESULT WriteFileAtomically(const std::wstring& path, std::span<const uint8_t> content);

HRESULT ReadSmallFile(const std::wstring& path, DWORD maxSize, std::vector<uint8_t>* content);

std::vector<std::wstring> ListFiles(const std::wstring& directory, const std::wstring& pattern);
