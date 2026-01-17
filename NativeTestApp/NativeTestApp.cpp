#include <Windows.h>
#include <combaseapi.h>
#include <iostream>
#include <string>

// Native COM CLSID/IID
// {94652869-D6DC-4D27-A3C6-2298F527AC4D}
static const CLSID CLSID_NativeTestCom =
{ 0x94652869, 0xd6dc, 0x4d27, { 0xa3, 0xc6, 0x22, 0x98, 0xf5, 0x27, 0xac, 0x4d } };

// {BB722A82-E4CE-4C01-8C73-751BCCE3A002}
static const IID IID_INativeTestCom =
{ 0xbb722a82, 0xe4ce, 0x4c01, { 0x8c, 0x73, 0x75, 0x1b, 0xcc, 0xe3, 0xa0, 0x02 } };

struct INativeTestCom : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Ping() = 0;
};

// .NET COM CLSID/IID
// {DD1FA7F8-D82B-490E-8E35-5D32182581EE}
static const CLSID CLSID_DotNetTestCom =
{ 0xdd1fa7f8, 0xd82b, 0x490e, { 0x8e, 0x35, 0x5d, 0x32, 0x18, 0x25, 0x81, 0xee } };

// {B92636AD-2ED3-4F92-B9F9-45B64729FB4F}
static const IID IID_IDotNetTestCom =
{ 0xb92636ad, 0x2ed3, 0x4f92, { 0xb9, 0xf9, 0x45, 0xb6, 0x47, 0x29, 0xfb, 0x4f } };

struct IDotNetTestCom : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Ping() = 0;
};

void TestRegistry() {
    const wchar_t* subKey = L"Software\\RealEBStarter\\TestApp";
    const wchar_t* valueName = L"SampleValue";
    const wchar_t* valueData = L"Hello from NativeTestApp";

    HKEY keyHandle = nullptr;
    DWORD disposition = 0;
    LONG status = RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr, 0,
                                 KEY_READ | KEY_WRITE, nullptr, &keyHandle, &disposition);
    if (status != ERROR_SUCCESS) {
        std::wcerr << L"Failed to create/open registry key: " << status << std::endl;
        return;
    }

    status = RegSetValueExW(keyHandle, valueName, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(valueData),
                            static_cast<DWORD>((wcslen(valueData) + 1) * sizeof(wchar_t)));
    if (status != ERROR_SUCCESS) {
        std::wcerr << L"Failed to write registry value: " << status << std::endl;
        RegCloseKey(keyHandle);
        return;
    }

    wchar_t buffer[256] = {};
    DWORD bufferSize = sizeof(buffer);
    DWORD type = 0;
    status = RegGetValueW(keyHandle, nullptr, valueName, RRF_RT_REG_SZ, &type, buffer, &bufferSize);
    if (status == ERROR_SUCCESS) {
        std::wcout << L"Registry read: " << buffer << std::endl;
    } else {
        std::wcerr << L"Failed to read registry value: " << status << std::endl;
    }

    RegCloseKey(keyHandle);
}

int wmain() {
    std::wcout << L"NativeTestApp starting..." << std::endl;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        std::wcerr << L"CoInitializeEx failed: 0x" << std::hex << hr << std::endl;
        return 1;
    }

    // Native COM
    INativeTestCom* nativeCom = nullptr;
    hr = CoCreateInstance(CLSID_NativeTestCom, nullptr, CLSCTX_INPROC_SERVER, IID_INativeTestCom,
                          reinterpret_cast<void**>(&nativeCom));
    if (SUCCEEDED(hr)) {
        nativeCom->Ping();
        nativeCom->Release();
        std::wcout << L"Native COM Ping succeeded" << std::endl;
    } else {
        std::wcerr << L"Native COM Ping failed: 0x" << std::hex << hr << std::endl;
    }

    // .NET COM
    IDotNetTestCom* dotnetCom = nullptr;
    hr = CoCreateInstance(CLSID_DotNetTestCom, nullptr, CLSCTX_INPROC_SERVER, IID_IDotNetTestCom,
                          reinterpret_cast<void**>(&dotnetCom));
    if (SUCCEEDED(hr)) {
        dotnetCom->Ping();
        dotnetCom->Release();
        std::wcout << L"DotNet COM Ping succeeded" << std::endl;
    } else {
        std::wcerr << L"DotNet COM Ping failed: 0x" << std::hex << hr << std::endl;
    }

    TestRegistry();
    CoUninitialize();

    std::wcout << L"NativeTestApp completed." << std::endl;
    return 0;
}
