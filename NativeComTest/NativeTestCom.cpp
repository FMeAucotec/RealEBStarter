#include "NativeTestCom.h"
#include <combaseapi.h>
#include <new>
#include <string>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {
    std::atomic<long> g_moduleRefCount{ 0 };
}

NativeTestCom::NativeTestCom() : m_refCount(1) {
    g_moduleRefCount.fetch_add(1);
}

HRESULT STDMETHODCALLTYPE NativeTestCom::QueryInterface(REFIID riid, void** ppvObject) {
    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == IID_IUnknown || riid == IID_INativeTestCom) {
        *ppvObject = static_cast<INativeTestCom*>(this);
        AddRef();
        return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE NativeTestCom::AddRef() {
    return static_cast<ULONG>(m_refCount.fetch_add(1) + 1);
}

ULONG STDMETHODCALLTYPE NativeTestCom::Release() {
    ULONG count = static_cast<ULONG>(m_refCount.fetch_sub(1) - 1);
    if (count == 0) {
        delete this;
    }
    return count;
}

HRESULT STDMETHODCALLTYPE NativeTestCom::Ping() {
    return S_OK;
}

NativeTestComFactory::NativeTestComFactory() : m_refCount(1) {
    g_moduleRefCount.fetch_add(1);
}

HRESULT STDMETHODCALLTYPE NativeTestComFactory::QueryInterface(REFIID riid, void** ppvObject) {
    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        *ppvObject = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE NativeTestComFactory::AddRef() {
    return static_cast<ULONG>(m_refCount.fetch_add(1) + 1);
}

ULONG STDMETHODCALLTYPE NativeTestComFactory::Release() {
    ULONG count = static_cast<ULONG>(m_refCount.fetch_sub(1) - 1);
    if (count == 0) {
        delete this;
    }
    return count;
}

HRESULT STDMETHODCALLTYPE NativeTestComFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) {
    if (pUnkOuter != nullptr) {
        return CLASS_E_NOAGGREGATION;
    }

    NativeTestCom* instance = new (std::nothrow) NativeTestCom();
    if (!instance) {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = instance->QueryInterface(riid, ppvObject);
    instance->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE NativeTestComFactory::LockServer(BOOL fLock) {
    if (fLock) {
        g_moduleRefCount.fetch_add(1);
    } else {
        g_moduleRefCount.fetch_sub(1);
    }
    return S_OK;
}

STDAPI DllCanUnloadNow(void) {
    return g_moduleRefCount.load() == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppvObject) {
    if (rclsid != CLSID_NativeTestCom) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    NativeTestComFactory* factory = new (std::nothrow) NativeTestComFactory();
    if (!factory) {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = factory->QueryInterface(riid, ppvObject);
    factory->Release();
    return hr;
}

HRESULT RegisterNativeTestCom() {
    wchar_t modulePath[MAX_PATH] = {};
    if (!GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    wchar_t clsidString[64] = {};
    StringFromGUID2(CLSID_NativeTestCom, clsidString, ARRAYSIZE(clsidString));

    std::wstring keyPath = L"Software\\Classes\\CLSID\\";
    keyPath += clsidString;

    HKEY clsidKey = nullptr;
    LONG status = RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr, 0,
                                 KEY_WRITE, nullptr, &clsidKey, nullptr);
    if (status != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(status);
    }

    const wchar_t* description = L"Native Test COM Server";
    RegSetValueExW(clsidKey, nullptr, 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(description),
                   static_cast<DWORD>((wcslen(description) + 1) * sizeof(wchar_t)));

    HKEY inprocKey = nullptr;
    status = RegCreateKeyExW(clsidKey, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &inprocKey, nullptr);
    if (status == ERROR_SUCCESS) {
        RegSetValueExW(inprocKey, nullptr, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(modulePath),
                       static_cast<DWORD>((wcslen(modulePath) + 1) * sizeof(wchar_t)));
        const wchar_t* threading = L"Both";
        RegSetValueExW(inprocKey, L"ThreadingModel", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(threading),
                       static_cast<DWORD>((wcslen(threading) + 1) * sizeof(wchar_t)));
        RegCloseKey(inprocKey);
    }

    RegCloseKey(clsidKey);
    return S_OK;
}

HRESULT UnregisterNativeTestCom() {
    wchar_t clsidString[64] = {};
    StringFromGUID2(CLSID_NativeTestCom, clsidString, ARRAYSIZE(clsidString));

    std::wstring keyPath = L"Software\\Classes\\CLSID\\";
    keyPath += clsidString;

    RegDeleteTreeW(HKEY_CURRENT_USER, keyPath.c_str());
    return S_OK;
}
