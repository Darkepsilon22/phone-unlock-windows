#pragma once

#include "fields.h"
#include "secure_vector.h"
#include <functional>
#include <string>

class Credential final : public ICredentialProviderCredential2
{
public:
    using ResultReporter = std::function<void(NTSTATUS status, NTSTATUS subStatus)>;

    static HRESULT Create(CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, ICredentialProviderUser* user,
        Credential** credential);

    std::wstring Sid() const;
    void ArmAutoLogon(SecureString password, ResultReporter reporter);

    IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Advise(ICredentialProviderCredentialEvents* events) override;
    IFACEMETHODIMP UnAdvise() override;
    IFACEMETHODIMP SetSelected(BOOL* autoLogon) override;
    IFACEMETHODIMP SetDeselected() override;
    IFACEMETHODIMP GetFieldState(DWORD fieldId, CREDENTIAL_PROVIDER_FIELD_STATE* visibility,
        CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* interactivity) override;
    IFACEMETHODIMP GetStringValue(DWORD fieldId, PWSTR* value) override;
    IFACEMETHODIMP GetBitmapValue(DWORD fieldId, HBITMAP* bitmap) override;
    IFACEMETHODIMP GetCheckboxValue(DWORD fieldId, BOOL* checked, PWSTR* label) override;
    IFACEMETHODIMP GetSubmitButtonValue(DWORD fieldId, DWORD* adjacentTo) override;
    IFACEMETHODIMP GetComboBoxValueCount(DWORD fieldId, DWORD* count, DWORD* selected) override;
    IFACEMETHODIMP GetComboBoxValueAt(DWORD fieldId, DWORD item, PWSTR* value) override;
    IFACEMETHODIMP SetStringValue(DWORD fieldId, PCWSTR value) override;
    IFACEMETHODIMP SetCheckboxValue(DWORD fieldId, BOOL checked) override;
    IFACEMETHODIMP SetComboBoxSelectedValue(DWORD fieldId, DWORD selected) override;
    IFACEMETHODIMP CommandLinkClicked(DWORD fieldId) override;
    IFACEMETHODIMP GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* response,
        CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* serialization, PWSTR* statusText,
        CREDENTIAL_PROVIDER_STATUS_ICON* statusIcon) override;
    IFACEMETHODIMP ReportResult(NTSTATUS status, NTSTATUS subStatus, PWSTR* statusText,
        CREDENTIAL_PROVIDER_STATUS_ICON* statusIcon) override;

    IFACEMETHODIMP GetUserSid(PWSTR* sid) override;

private:
    Credential();
    ~Credential();

    HRESULT Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, ICredentialProviderUser* user);
    HRESULT Serialize(PCWSTR protectedPassword, CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* serialization) const;
    void ShowStatus(PCWSTR text);
    void ClearPassword();

    LONG refCount_;
    CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario_;
    ICredentialProviderCredentialEvents* events_;
    PWSTR fieldValues_[FieldCount];
    PWSTR userSid_;
    PWSTR qualifiedUserName_;
    bool isLocalAccount_;
    SecureString autoLogonPassword_;
    ResultReporter reportResult_;
};
