#include "provider.h"

namespace
{
LONG dllRefCount = 0;

class ClassFactory final : public IClassFactory
{
public:
    ClassFactory() : refCount_(1)
    {
    }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override
    {
        static const QITAB interfaces[] =
        {
            QITABENT(ClassFactory, IClassFactory),
            {},
        };
        return QISearch(this, interfaces, riid, object);
    }

    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&refCount_));
    }

    IFACEMETHODIMP_(ULONG) Release() override
    {
        const LONG count = InterlockedDecrement(&refCount_);
        if (count == 0)
        {
            delete this;
        }
        return static_cast<ULONG>(count);
    }

    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** object) override
    {
        *object = nullptr;
        return outer ? CLASS_E_NOAGGREGATION : Provider::CreateInstance(riid, object);
    }

    IFACEMETHODIMP LockServer(BOOL lock) override
    {
        lock ? DllAddRef() : DllRelease();
        return S_OK;
    }

private:
    ~ClassFactory() = default;

    LONG refCount_;
};
}

void DllAddRef()
{
    InterlockedIncrement(&dllRefCount);
}

void DllRelease()
{
    InterlockedDecrement(&dllRefCount);
}

STDAPI DllCanUnloadNow()
{
    return dllRefCount > 0 ? S_FALSE : S_OK;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** object)
{
    *object = nullptr;
    if (!IsEqualCLSID(clsid, CLSID_PhoneUnlockProtoProvider))
    {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* factory = new (std::nothrow) ClassFactory();
    if (!factory)
    {
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = factory->QueryInterface(riid, object);
    factory->Release();
    return hr;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
