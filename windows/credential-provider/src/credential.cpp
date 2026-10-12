#include "credential.h"
#include "secure_string.h"
#include "serialization.h"
#include "tile_bitmap.h"

namespace
{
constexpr wchar_t LabelText[] = L"Téléphone";
constexpr wchar_t SubmitText[] = L"Envoyer";
constexpr wchar_t InitialStatus[] = L"Saisis le mot de passe du compte de test.";
constexpr wchar_t EmptyPasswordStatus[] = L"Mot de passe vide.";
constexpr wchar_t AcceptedStatus[] = L"Windows a accepté les identifiants.";
constexpr wchar_t PreparationFailedFormat[] = L"Échec de préparation : 0x%08X";
constexpr wchar_t RejectedFormat[] = L"Refus Windows : 0x%08X (0x%08X)";
constexpr size_t StatusLength = 128;

PCWSTR InitialFieldValue(DWORD fieldId)
{
    switch (fieldId)
    {
    case LabelField:
        return LabelText;
    case StatusField:
        return InitialStatus;
    case SubmitField:
        return SubmitText;
    default:
        return L"";
    }
}

unsigned AsHex(LONG value)
{
    return static_cast<unsigned>(value);
}
}

Credential::Credential()
    : refCount_(1),
      scenario_(CPUS_INVALID),
      events_(nullptr),
      fieldValues_{},
      userSid_(nullptr),
      qualifiedUserName_(nullptr),
      isLocalAccount_(false)
{
    DllAddRef();
}

Credential::~Credential()
{
    SecureFree(&fieldValues_[PasswordField]);
    for (PWSTR& value : fieldValues_)
    {
        CoTaskMemFree(value);
    }
    CoTaskMemFree(userSid_);
    CoTaskMemFree(qualifiedUserName_);
    if (events_)
    {
        events_->Release();
    }
    DllRelease();
}

HRESULT Credential::Create(CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, ICredentialProviderUser* user,
    Credential** credential)
{
    *credential = nullptr;
    auto* instance = new (std::nothrow) Credential();
    if (!instance)
    {
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = instance->Initialize(scenario, user);
    if (FAILED(hr))
    {
        instance->Release();
        return hr;
    }
    *credential = instance;
    return S_OK;
}

HRESULT Credential::Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, ICredentialProviderUser* user)
{
    scenario_ = scenario;
    GUID accountProvider = GUID_NULL;
    HRESULT hr = user->GetStringValue(PKEY_Identity_QualifiedUserName, &qualifiedUserName_);
    if (SUCCEEDED(hr))
    {
        hr = user->GetSid(&userSid_);
    }
    if (SUCCEEDED(hr))
    {
        hr = user->GetProviderID(&accountProvider);
    }
    isLocalAccount_ = IsEqualGUID(accountProvider, Identity_LocalUserProvider) != FALSE;
    for (DWORD fieldId = 0; SUCCEEDED(hr) && fieldId < FieldCount; ++fieldId)
    {
        hr = SHStrDupW(InitialFieldValue(fieldId), &fieldValues_[fieldId]);
    }
    LogLine(L"credential init hr=0x%08X local=%d", AsHex(hr), isLocalAccount_);
    return hr;
}

IFACEMETHODIMP Credential::QueryInterface(REFIID riid, void** object)
{
    static const QITAB interfaces[] =
    {
        QITABENT(Credential, ICredentialProviderCredential),
        QITABENT(Credential, ICredentialProviderCredential2),
        {},
    };
    return QISearch(this, interfaces, riid, object);
}

IFACEMETHODIMP_(ULONG) Credential::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&refCount_));
}

IFACEMETHODIMP_(ULONG) Credential::Release()
{
    const LONG count = InterlockedDecrement(&refCount_);
    if (count == 0)
    {
        delete this;
    }
    return static_cast<ULONG>(count);
}

IFACEMETHODIMP Credential::Advise(ICredentialProviderCredentialEvents* events)
{
    if (events_)
    {
        events_->Release();
    }
    events_ = events;
    if (events_)
    {
        events_->AddRef();
    }
    return S_OK;
}

IFACEMETHODIMP Credential::UnAdvise()
{
    return Advise(nullptr);
}

IFACEMETHODIMP Credential::SetSelected(BOOL* autoLogon)
{
    *autoLogon = FALSE;
    return S_OK;
}

IFACEMETHODIMP Credential::SetDeselected()
{
    ClearPassword();
    return S_OK;
}

IFACEMETHODIMP Credential::GetFieldState(DWORD fieldId, CREDENTIAL_PROVIDER_FIELD_STATE* visibility,
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* interactivity)
{
    if (!IsValidField(fieldId))
    {
        return E_INVALIDARG;
    }
    *visibility = FieldStates[fieldId].visibility;
    *interactivity = FieldStates[fieldId].interactivity;
    return S_OK;
}

IFACEMETHODIMP Credential::GetStringValue(DWORD fieldId, PWSTR* value)
{
    *value = nullptr;
    if (!IsValidField(fieldId) || fieldId == TileImageField)
    {
        return E_INVALIDARG;
    }
    return SHStrDupW(fieldValues_[fieldId], value);
}

IFACEMETHODIMP Credential::GetBitmapValue(DWORD fieldId, HBITMAP* bitmap)
{
    *bitmap = nullptr;
    if (fieldId != TileImageField)
    {
        return E_INVALIDARG;
    }
    *bitmap = CreateTileBitmap();
    return *bitmap ? S_OK : E_OUTOFMEMORY;
}

IFACEMETHODIMP Credential::GetCheckboxValue(DWORD, BOOL*, PWSTR*)
{
    return E_NOTIMPL;
}

IFACEMETHODIMP Credential::GetSubmitButtonValue(DWORD fieldId, DWORD* adjacentTo)
{
    if (fieldId != SubmitField)
    {
        return E_INVALIDARG;
    }
    *adjacentTo = PasswordField;
    return S_OK;
}

IFACEMETHODIMP Credential::GetComboBoxValueCount(DWORD, DWORD*, DWORD*)
{
    return E_NOTIMPL;
}

IFACEMETHODIMP Credential::GetComboBoxValueAt(DWORD, DWORD, PWSTR*)
{
    return E_NOTIMPL;
}

IFACEMETHODIMP Credential::SetStringValue(DWORD fieldId, PCWSTR value)
{
    if (fieldId != PasswordField)
    {
        return E_INVALIDARG;
    }
    SecureFree(&fieldValues_[PasswordField]);
    return SHStrDupW(value, &fieldValues_[PasswordField]);
}

IFACEMETHODIMP Credential::SetCheckboxValue(DWORD, BOOL)
{
    return E_NOTIMPL;
}

IFACEMETHODIMP Credential::SetComboBoxSelectedValue(DWORD, DWORD)
{
    return E_NOTIMPL;
}

IFACEMETHODIMP Credential::CommandLinkClicked(DWORD)
{
    return E_NOTIMPL;
}

IFACEMETHODIMP Credential::GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* response,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* serialization, PWSTR* statusText,
    CREDENTIAL_PROVIDER_STATUS_ICON* statusIcon)
{
    *response = CPGSR_NO_CREDENTIAL_NOT_FINISHED;
    *statusText = nullptr;
    *statusIcon = CPSI_NONE;
    ZeroMemory(serialization, sizeof(*serialization));

    PCWSTR password = fieldValues_[PasswordField];
    if (!password || *password == L'\0')
    {
        ShowStatus(EmptyPasswordStatus);
        return S_OK;
    }

    PWSTR protectedPassword = nullptr;
    HRESULT hr = CopyProtectedPassword(password, scenario_, &protectedPassword);
    if (SUCCEEDED(hr))
    {
        hr = Serialize(protectedPassword, serialization);
    }
    SecureFree(&protectedPassword);
    ClearPassword();
    LogLine(L"serialization hr=0x%08X scenario=%d local=%d", AsHex(hr), scenario_, isLocalAccount_);

    if (FAILED(hr))
    {
        wchar_t message[StatusLength];
        StringCchPrintfW(message, StatusLength, PreparationFailedFormat, AsHex(hr));
        ShowStatus(message);
        return S_OK;
    }
    *response = CPGSR_RETURN_CREDENTIAL_FINISHED;
    return S_OK;
}

HRESULT Credential::Serialize(PCWSTR protectedPassword,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* serialization) const
{
    ULONG packageId = 0;
    HRESULT hr = GetNegotiatePackageId(&packageId);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = isLocalAccount_
        ? SerializeLocalAccountLogon(qualifiedUserName_, protectedPassword, scenario_,
              &serialization->rgbSerialization, &serialization->cbSerialization)
        : SerializeProviderAccountLogon(qualifiedUserName_, protectedPassword,
              &serialization->rgbSerialization, &serialization->cbSerialization);
    if (FAILED(hr))
    {
        return hr;
    }
    serialization->ulAuthenticationPackage = packageId;
    serialization->clsidCredentialProvider = CLSID_PhoneUnlockProtoProvider;
    return S_OK;
}

IFACEMETHODIMP Credential::ReportResult(NTSTATUS status, NTSTATUS subStatus, PWSTR* statusText,
    CREDENTIAL_PROVIDER_STATUS_ICON* statusIcon)
{
    *statusText = nullptr;
    *statusIcon = CPSI_NONE;
    LogLine(L"result status=0x%08X substatus=0x%08X scenario=%d local=%d",
        AsHex(status), AsHex(subStatus), scenario_, isLocalAccount_);
    ClearPassword();

    if (status >= 0)
    {
        ShowStatus(AcceptedStatus);
        return S_OK;
    }

    wchar_t message[StatusLength];
    StringCchPrintfW(message, StatusLength, RejectedFormat, AsHex(status), AsHex(subStatus));
    ShowStatus(message);
    *statusIcon = CPSI_ERROR;
    return SHStrDupW(message, statusText);
}

IFACEMETHODIMP Credential::GetUserSid(PWSTR* sid)
{
    *sid = nullptr;
    return userSid_ ? SHStrDupW(userSid_, sid) : E_UNEXPECTED;
}

void Credential::ShowStatus(PCWSTR text)
{
    PWSTR copy = nullptr;
    if (SUCCEEDED(SHStrDupW(text, &copy)))
    {
        CoTaskMemFree(fieldValues_[StatusField]);
        fieldValues_[StatusField] = copy;
    }
    if (events_)
    {
        events_->SetFieldString(this, StatusField, text);
    }
}

void Credential::ClearPassword()
{
    SecureFree(&fieldValues_[PasswordField]);
    SHStrDupW(L"", &fieldValues_[PasswordField]);
    if (events_)
    {
        events_->SetFieldString(this, PasswordField, L"");
    }
}
