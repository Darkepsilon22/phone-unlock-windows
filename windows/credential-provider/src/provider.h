#pragma once

#include "credential.h"
#include <vector>

class Provider final : public ICredentialProvider, public ICredentialProviderSetUserArray
{
public:
    static HRESULT CreateInstance(REFIID riid, void** object);

    IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, DWORD flags) override;
    IFACEMETHODIMP SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* serialization) override;
    IFACEMETHODIMP Advise(ICredentialProviderEvents* events, UINT_PTR context) override;
    IFACEMETHODIMP UnAdvise() override;
    IFACEMETHODIMP GetFieldDescriptorCount(DWORD* count) override;
    IFACEMETHODIMP GetFieldDescriptorAt(DWORD index, CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** descriptor) override;
    IFACEMETHODIMP GetCredentialCount(DWORD* count, DWORD* defaultIndex, BOOL* autoLogonWithDefault) override;
    IFACEMETHODIMP GetCredentialAt(DWORD index, ICredentialProviderCredential** credential) override;

    IFACEMETHODIMP SetUserArray(ICredentialProviderUserArray* users) override;

private:
    Provider();
    ~Provider();

    void RebuildCredentials();
    HRESULT AddCredentialFor(DWORD userIndex);
    void ReleaseCredentials();

    LONG refCount_;
    CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario_;
    ICredentialProviderUserArray* users_;
    std::vector<Credential*> credentials_;
    bool credentialsOutdated_;
};
