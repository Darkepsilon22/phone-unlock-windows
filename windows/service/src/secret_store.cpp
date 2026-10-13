#include "secret_store.h"
#include <dpapi.h>
#include <sddl.h>
#include <algorithm>

namespace
{
constexpr wchar_t SecretDescription[] = L"PhoneUnlock";
constexpr wchar_t SecretExtension[] = L".bin";
constexpr wchar_t TemporaryExtension[] = L".tmp";
constexpr DWORD MaxSecretFileSize = 16 * 1024;

class LocalBlob
{
public:
    LocalBlob() = default;
    LocalBlob(const LocalBlob&) = delete;
    LocalBlob& operator=(const LocalBlob&) = delete;

    ~LocalBlob()
    {
        if (blob.pbData)
        {
            SecureZeroMemory(blob.pbData, blob.cbData);
            LocalFree(blob.pbData);
        }
    }

    DATA_BLOB blob = {};
};

DATA_BLOB EntropyFor(const std::wstring& sid)
{
    return { static_cast<DWORD>(sid.size() * sizeof(wchar_t)),
        reinterpret_cast<BYTE*>(const_cast<wchar_t*>(sid.data())) };
}

HRESULT WriteWholeFile(const std::wstring& path, const DATA_BLOB& content)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    DWORD written = 0;
    const bool ok = WriteFile(file, content.pbData, content.cbData, &written, nullptr) &&
        written == content.cbData && FlushFileBuffers(file);
    const HRESULT hr = ok ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    CloseHandle(file);
    return hr;
}

HRESULT ReadWholeFile(const std::wstring& path, SecureBytes* content)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    LARGE_INTEGER size = {};
    HRESULT hr = GetFileSizeEx(file, &size) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    if (SUCCEEDED(hr) && (size.QuadPart <= 0 || size.QuadPart > MaxSecretFileSize))
    {
        hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    if (SUCCEEDED(hr))
    {
        const auto length = static_cast<DWORD>(size.QuadPart);
        *content = SecureBytes(length);
        content->ResizeWithinCapacity(length);
        DWORD read = 0;
        if (!ReadFile(file, content->data(), length, &read, nullptr) || read != length)
        {
            hr = HRESULT_FROM_WIN32(ERROR_READ_FAULT);
        }
    }
    CloseHandle(file);
    return hr;
}
}

bool IsValidSid(const std::wstring& sid)
{
    const bool safeCharacters = sid.size() > 4 && sid.starts_with(L"S-1-") &&
        std::all_of(sid.begin() + 2, sid.end(), [](wchar_t c) { return c == L'-' || (c >= L'0' && c <= L'9'); });
    if (!safeCharacters)
    {
        return false;
    }
    PSID parsed = nullptr;
    if (!ConvertStringSidToSidW(sid.c_str(), &parsed))
    {
        return false;
    }
    LocalFree(parsed);
    return true;
}

SecretStore::SecretStore(std::wstring directory) : directory_(std::move(directory))
{
}

HRESULT SecretStore::Save(const std::wstring& sid, const SecureString& password) const
{
    if (!IsValidSid(sid) || password.empty())
    {
        return E_INVALIDARG;
    }

    DATA_BLOB plain = { static_cast<DWORD>(password.length() * sizeof(wchar_t)),
        reinterpret_cast<BYTE*>(const_cast<wchar_t*>(password.c_str())) };
    DATA_BLOB entropy = EntropyFor(sid);
    LocalBlob encrypted;
    if (!CryptProtectData(&plain, SecretDescription, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
            &encrypted.blob))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    const std::wstring path = PathFor(sid);
    const std::wstring temporary = path + TemporaryExtension;
    HRESULT hr = WriteWholeFile(temporary, encrypted.blob);
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

HRESULT SecretStore::Load(const std::wstring& sid, SecureString* password) const
{
    if (!IsValidSid(sid))
    {
        return E_INVALIDARG;
    }

    SecureBytes stored;
    HRESULT hr = ReadWholeFile(PathFor(sid), &stored);
    if (FAILED(hr))
    {
        return hr;
    }

    DATA_BLOB encrypted = { static_cast<DWORD>(stored.size()), stored.data() };
    DATA_BLOB entropy = EntropyFor(sid);
    LocalBlob plain;
    if (!CryptUnprotectData(&encrypted, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &plain.blob))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    *password = SecureString(reinterpret_cast<const wchar_t*>(plain.blob.pbData), plain.blob.cbData / sizeof(wchar_t));
    return S_OK;
}

std::wstring SecretStore::PathFor(const std::wstring& sid) const
{
    return directory_ + L"\\" + sid + SecretExtension;
}
