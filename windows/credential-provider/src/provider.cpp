#include "provider.h"
#include <algorithm>

namespace
{
constexpr wchar_t LogDirectory[] = L"%ProgramData%\\PhoneUnlockProto";
constexpr wchar_t LogFileName[] = L"cp.log";
constexpr ULONGLONG PendingLogonLifetime = 30'000;

bool IsSupported(CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario)
{
    return scenario == CPUS_LOGON || scenario == CPUS_UNLOCK_WORKSTATION;
}
}

Provider::Provider()
    : refCount_(1),
      scenario_(CPUS_INVALID),
      users_(nullptr),
      credentialsOutdated_(true),
      events_(nullptr),
      adviseContext_(0)
{
    DllAddRef();
}

Provider::~Provider()
{
    StopListener();
    if (events_)
    {
        events_->Release();
    }
    ReleaseCredentials();
    if (users_)
    {
        users_->Release();
    }
    DllRelease();
}

HRESULT Provider::CreateInstance(REFIID riid, void** object)
{
    *object = nullptr;
    ConfigureLog(LogDirectory, LogFileName);
    auto* provider = new (std::nothrow) Provider();
    if (!provider)
    {
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = provider->QueryInterface(riid, object);
    provider->Release();
    return hr;
}

IFACEMETHODIMP Provider::QueryInterface(REFIID riid, void** object)
{
    static const QITAB interfaces[] =
    {
        QITABENT(Provider, ICredentialProvider),
        QITABENT(Provider, ICredentialProviderSetUserArray),
        {},
    };
    return QISearch(this, interfaces, riid, object);
}

IFACEMETHODIMP_(ULONG) Provider::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&refCount_));
}

IFACEMETHODIMP_(ULONG) Provider::Release()
{
    const LONG count = InterlockedDecrement(&refCount_);
    if (count == 0)
    {
        delete this;
    }
    return static_cast<ULONG>(count);
}

IFACEMETHODIMP Provider::SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario, DWORD)
{
    const bool supported = IsSupported(scenario);
    LogLine(L"usage scenario=%d supported=%d", scenario, supported);
    if (!supported)
    {
        return E_NOTIMPL;
    }
    scenario_ = scenario;
    credentialsOutdated_ = true;
    return S_OK;
}

IFACEMETHODIMP Provider::SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION*)
{
    return E_NOTIMPL;
}

IFACEMETHODIMP Provider::Advise(ICredentialProviderEvents* events, UINT_PTR context)
{
    std::lock_guard lock(mutex_);
    if (events_)
    {
        events_->Release();
    }
    events_ = events;
    adviseContext_ = context;
    if (events_)
    {
        events_->AddRef();
    }
    return S_OK;
}

IFACEMETHODIMP Provider::UnAdvise()
{
    StopListener();
    std::lock_guard lock(mutex_);
    if (events_)
    {
        events_->Release();
        events_ = nullptr;
    }
    adviseContext_ = 0;
    pending_.reset();
    return S_OK;
}

IFACEMETHODIMP Provider::GetFieldDescriptorCount(DWORD* count)
{
    *count = FieldCount;
    return S_OK;
}

IFACEMETHODIMP Provider::GetFieldDescriptorAt(DWORD index, CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** descriptor)
{
    return CopyFieldDescriptor(index, descriptor);
}

IFACEMETHODIMP Provider::GetCredentialCount(DWORD* count, DWORD* defaultIndex, BOOL* autoLogonWithDefault)
{
    *defaultIndex = CREDENTIAL_PROVIDER_NO_DEFAULT;
    *autoLogonWithDefault = FALSE;
    if (credentialsOutdated_)
    {
        RebuildCredentials();
        StartListener();
    }
    ArmPendingLogon(defaultIndex, autoLogonWithDefault);
    *count = static_cast<DWORD>(credentials_.size());
    return S_OK;
}

IFACEMETHODIMP Provider::GetCredentialAt(DWORD index, ICredentialProviderCredential** credential)
{
    *credential = nullptr;
    if (index >= credentials_.size())
    {
        return E_INVALIDARG;
    }
    return credentials_[index]->QueryInterface(IID_PPV_ARGS(credential));
}

IFACEMETHODIMP Provider::SetUserArray(ICredentialProviderUserArray* users)
{
    if (users_)
    {
        users_->Release();
    }
    users_ = users;
    if (users_)
    {
        users_->AddRef();
    }
    credentialsOutdated_ = true;
    return S_OK;
}

void Provider::RebuildCredentials()
{
    ReleaseCredentials();
    credentialsOutdated_ = false;

    DWORD userCount = 0;
    if (!users_ || FAILED(users_->GetCount(&userCount)))
    {
        LogLine(L"no user array");
        return;
    }
    for (DWORD userIndex = 0; userIndex < userCount; ++userIndex)
    {
        const HRESULT hr = AddCredentialFor(userIndex);
        if (FAILED(hr))
        {
            LogLine(L"user %lu skipped hr=0x%08X", userIndex, static_cast<unsigned>(hr));
        }
    }
    LogLine(L"credentials=%zu users=%lu", credentials_.size(), userCount);
}

HRESULT Provider::AddCredentialFor(DWORD userIndex)
{
    ICredentialProviderUser* user = nullptr;
    HRESULT hr = users_->GetAt(userIndex, &user);
    if (FAILED(hr))
    {
        return hr;
    }

    Credential* credential = nullptr;
    hr = Credential::Create(scenario_, user, &credential);
    user->Release();
    if (FAILED(hr))
    {
        return hr;
    }

    try
    {
        credentials_.push_back(credential);
    }
    catch (const std::bad_alloc&)
    {
        credential->Release();
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

void Provider::ReleaseCredentials()
{
    for (Credential* credential : credentials_)
    {
        credential->Release();
    }
    credentials_.clear();
}

void Provider::StartListener()
{
    std::vector<std::wstring> sids;
    for (const Credential* credential : credentials_)
    {
        sids.push_back(credential->Sid());
    }

    {
        std::lock_guard lock(mutex_);
        if (!events_ || sids.empty() || (listener_ && listener_->Sids() == sids))
        {
            return;
        }
    }

    StopListener();
    const auto scenario = scenario_ == CPUS_UNLOCK_WORKSTATION ? pipe_protocol::TileScenario::Unlock
                                                               : pipe_protocol::TileScenario::Logon;
    auto listener = std::make_shared<ServiceListener>(scenario, std::move(sids),
        [this](const std::wstring& sid, SecureString password) { OnServiceReady(sid, std::move(password)); });
    std::lock_guard lock(mutex_);
    listener_ = std::move(listener);
}

void Provider::StopListener()
{
    std::shared_ptr<ServiceListener> listener;
    {
        std::lock_guard lock(mutex_);
        listener.swap(listener_);
    }
}

void Provider::OnServiceReady(const std::wstring& sid, SecureString password)
{
    ICredentialProviderEvents* events = nullptr;
    UINT_PTR context = 0;
    {
        std::lock_guard lock(mutex_);
        pending_ = PendingLogon{ sid, std::move(password), GetTickCount64() };
        events = events_;
        context = adviseContext_;
        if (events)
        {
            events->AddRef();
        }
    }
    if (events)
    {
        events->CredentialsChanged(context);
        events->Release();
    }
}

void Provider::ArmPendingLogon(DWORD* defaultIndex, BOOL* autoLogonWithDefault)
{
    std::lock_guard lock(mutex_);
    if (!pending_)
    {
        return;
    }
    if (GetTickCount64() - pending_->receivedAt > PendingLogonLifetime)
    {
        LogLine(L"automatic logon expired");
        pending_.reset();
        return;
    }

    const auto match = std::find_if(credentials_.begin(), credentials_.end(),
        [this](const Credential* credential) { return credential->Sid() == pending_->sid; });
    if (match != credentials_.end())
    {
        std::weak_ptr<ServiceListener> listener = listener_;
        const std::wstring sid = pending_->sid;
        (*match)->ArmAutoLogon(std::move(pending_->password), [listener, sid](NTSTATUS status, NTSTATUS subStatus) {
            if (const auto alive = listener.lock())
            {
                alive->ReportResult(sid, status, subStatus);
            }
        });
        *defaultIndex = static_cast<DWORD>(match - credentials_.begin());
        *autoLogonWithDefault = TRUE;
    }
    pending_.reset();
}
