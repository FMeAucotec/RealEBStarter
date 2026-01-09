#pragma once
#include <Windows.h>
#include <combaseapi.h>

// Function prototype for original CoCreateInstance
typedef HRESULT (STDAPICALLTYPE *CoCreateInstance_t)(
    REFCLSID rclsid,
    LPUNKNOWN pUnkOuter,
    DWORD dwClsContext,
    REFIID riid,
    LPVOID* ppv
);

// Function prototype for original CoCreateInstanceEx
typedef HRESULT (STDAPICALLTYPE *CoCreateInstanceEx_t)(
    REFCLSID rclsid,
    IUnknown* punkOuter,
    DWORD dwClsCtx,
    COSERVERINFO* pServerInfo,
    DWORD dwCount,
    MULTI_QI* pResults
);

// Function prototype for original CoGetClassObject
typedef HRESULT (STDAPICALLTYPE *CoGetClassObject_t)(
    REFCLSID rclsid,
    DWORD dwClsContext,
    COSERVERINFO* pServerInfo,
    REFIID riid,
    LPVOID* ppv
);

// Hooked functions
HRESULT STDAPICALLTYPE Hooked_CoCreateInstance(
    REFCLSID rclsid,
    LPUNKNOWN pUnkOuter,
    DWORD dwClsContext,
    REFIID riid,
    LPVOID* ppv
);

HRESULT STDAPICALLTYPE Hooked_CoCreateInstanceEx(
    REFCLSID rclsid,
    IUnknown* punkOuter,
    DWORD dwClsCtx,
    COSERVERINFO* pServerInfo,
    DWORD dwCount,
    MULTI_QI* pResults
);

HRESULT STDAPICALLTYPE Hooked_CoGetClassObject(
    REFCLSID rclsid,
    DWORD dwClsContext,
    COSERVERINFO* pServerInfo,
    REFIID riid,
    LPVOID* ppv
);

// Hook installation/removal
bool InstallCOMHooks();
bool RemoveCOMHooks();

// Original function pointers
extern CoCreateInstance_t Real_CoCreateInstance;
extern CoCreateInstanceEx_t Real_CoCreateInstanceEx;
extern CoGetClassObject_t Real_CoGetClassObject;
