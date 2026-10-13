#include "file_io.h"

namespace
{
constexpr wchar_t TemporaryExtension[] = L".tmp";

HRESULT WriteWholeFile(const std::wstring& path, std::span<const uint8_t> content)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    DWORD written = 0;
    const bool ok = WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) &&
        written == content.size() && FlushFileBuffers(file);
    const HRESULT hr = ok ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    CloseHandle(file);
    return hr;
}
}

HRESULT WriteFileAtomically(const std::wstring& path, std::span<const uint8_t> content)
{
    const std::wstring temporary = path + TemporaryExtension;
    HRESULT hr = WriteWholeFile(temporary, content);
    if (SUCCEEDED(hr) && !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }
    if (FAILED(hr))
    {
        DeleteFileW(temporary.c_str());
    }
    return hr;
}

HRESULT ReadSmallFile(const std::wstring& path, DWORD maxSize, std::vector<uint8_t>* content)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    LARGE_INTEGER size = {};
    HRESULT hr = GetFileSizeEx(file, &size) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    if (SUCCEEDED(hr) && (size.QuadPart <= 0 || size.QuadPart > maxSize))
    {
        hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    if (SUCCEEDED(hr))
    {
        content->resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        if (!ReadFile(file, content->data(), static_cast<DWORD>(content->size()), &read, nullptr) ||
            read != content->size())
        {
            hr = HRESULT_FROM_WIN32(ERROR_READ_FAULT);
        }
    }
    CloseHandle(file);
    return hr;
}

std::vector<std::wstring> ListFiles(const std::wstring& directory, const std::wstring& pattern)
{
    std::vector<std::wstring> paths;
    WIN32_FIND_DATAW entry = {};
    HANDLE search = FindFirstFileW((directory + L"\\" + pattern).c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE)
    {
        return paths;
    }
    do
    {
        if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            paths.push_back(directory + L"\\" + entry.cFileName);
        }
    } while (FindNextFileW(search, &entry));
    FindClose(search);
    return paths;
}
