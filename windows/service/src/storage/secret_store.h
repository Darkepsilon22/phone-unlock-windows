#pragma once

#include "secure_vector.h"
#include <string>

bool IsValidSid(const std::wstring& sid);

class SecretStore
{
public:
    explicit SecretStore(std::wstring directory);

    HRESULT Save(const std::wstring& sid, const SecureString& password) const;
    HRESULT Load(const std::wstring& sid, SecureString* password) const;
    bool Exists(const std::wstring& sid) const;

private:
    std::wstring PathFor(const std::wstring& sid) const;

    std::wstring directory_;
};
