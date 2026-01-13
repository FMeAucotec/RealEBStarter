#include "pch.h"
#include "COMHooks.h"
#include "COMConfig.h"

// MS Detours external declarations
extern "C" {
    LONG WINAPI DetourTransactionBegin();
    LONG WINAPI DetourUpdateThread(HANDLE hThread);
    LONG WINAPI DetourAttach(PVOID* ppPointer, PVOID pDetour);
    LONG WINAPI DetourDetach(PVOID* ppPointer, PVOID pDetour);
    LONG WINAPI DetourTransactionCommit();
}

// Original function pointers - get from ole32.dll
static CoCreateInstance_t GetRealCoCreateInstance() {
    HMODULE hOle32 = GetModuleHandleW(L"ole32.dll");
    return hOle32 ? reinterpret_cast<CoCreateInstance_t>(GetProcAddress(hOle32, "CoCreateInstance")) : nullptr;
}

static CoCreateInstanceEx_t GetRealCoCreateInstanceEx() {
    HMODULE hOle32 = GetModuleHandleW(L"ole32.dll");
    return hOle32 ? reinterpret_cast<CoCreateInstanceEx_t>(GetProcAddress(hOle32, "CoCreateInstanceEx")) : nullptr;
}

static CoGetClassObject_t GetRealCoGetClassObject() {
    HMODULE hOle32 = GetModuleHandleW(L"ole32.dll");
    return hOle32 ? reinterpret_cast<CoGetClassObject_t>(GetProcAddress(hOle32, "CoGetClassObject")) : nullptr;
}

CoCreateInstance_t Real_CoCreateInstance = GetRealCoCreateInstance();
CoCreateInstanceEx_t Real_CoCreateInstanceEx = GetRealCoCreateInstanceEx();
CoGetClassObject_t Real_CoGetClassObject = GetRealCoGetClassObject();

// Helper to convert CLSID to string for debugging
std::wstring CLSIDToDebugString(REFCLSID rclsid) {
    LPOLESTR clsidStr = nullptr;
    if (SUCCEEDED(StringFromCLSID(rclsid, &clsidStr))) {
        std::wstring result(clsidStr);
        CoTaskMemFree(clsidStr);
        return result;
    }
    return L"{Unknown}";
}

// Hooked CoCreateInstance
HRESULT STDAPICALLTYPE Hooked_CoCreateInstance(
    REFCLSID rclsid,
    LPUNKNOWN pUnkOuter,
    DWORD dwClsContext,
    REFIID riid,
    LPVOID* ppv
) {
    // Check if this CLSID should be redirected
    if (COMConfig::GetInstance().HasRedirectedCLSID(rclsid)) {
        // Load DLL and create instance
        if (COMConfig::GetInstance().LoadDllAndGetClassObject(rclsid, riid, ppv)) {
            return S_OK;
        }
        // If failed to load from our DLL, fall through to original
    }
    
    // Not redirected or failed - use original COM
    return Real_CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
}

// Hooked CoCreateInstanceEx
HRESULT STDAPICALLTYPE Hooked_CoCreateInstanceEx(
    REFCLSID rclsid,
    IUnknown* punkOuter,
    DWORD dwClsCtx,
    COSERVERINFO* pServerInfo,
    DWORD dwCount,
    MULTI_QI* pResults
) {
    // Check if this CLSID should be redirected
    if (COMConfig::GetInstance().HasRedirectedCLSID(rclsid)) {
        LPVOID pInstance = nullptr;
        // For CoCreateInstanceEx, we need to handle multiple interfaces
        if (dwCount > 0 && pResults) {
            if (punkOuter == nullptr && (dwCount & CLSCTX_REMOTE_SERVER) == CLSCTX_REMOTE_SERVER)   { 
                if (COMConfig::GetInstance().ExecuteShellCommand(rclsid)) {
                    // maybe a wait is necessary ...
                    // remote possible via PsExec from Sysinternals ???
                }

            }
            // Try to create instance with first interface
            else if (COMConfig::GetInstance().LoadDllAndGetClassObject(rclsid, pResults[0].pIID ? *pResults[0].pIID : IID_IUnknown, &pInstance)) {
                // Query for all requested interfaces
                bool allSucceeded = true;
                for (DWORD i = 0; i < dwCount; i++) {
                    if (pResults[i].pIID) {
                        pResults[i].hr = static_cast<IUnknown*>(pInstance)->QueryInterface(*pResults[i].pIID, (void**)&pResults[i].pItf);
                        if (FAILED(pResults[i].hr)) {
                            allSucceeded = false;
                        }
                    }
                }
                
                // Release the initial instance
                static_cast<IUnknown*>(pInstance)->Release();
                
                return allSucceeded ? S_OK : CO_S_NOTALLINTERFACES;
            }
        }
    }
    
    // Not redirected or failed - use original COM
    return Real_CoCreateInstanceEx(rclsid, punkOuter, dwClsCtx, pServerInfo, dwCount, pResults);
}

// Hooked CoGetClassObject
HRESULT STDAPICALLTYPE Hooked_CoGetClassObject(
    REFCLSID rclsid,
    DWORD dwClsContext,
    COSERVERINFO* pServerInfo,
    REFIID riid,
    LPVOID* ppv
) {
    // Check if this CLSID should be redirected
    if (COMConfig::GetInstance().HasRedirectedCLSID(rclsid)) {
        std::wstring dllPath;
        if (COMConfig::GetInstance().GetDllPath(rclsid, dllPath)) {
            // Load the DLL
            HMODULE hDll = LoadLibraryW(dllPath.c_str());
            if (hDll) {
                // Get DllGetClassObject function
                typedef HRESULT (STDAPICALLTYPE *DllGetClassObjectFunc)(REFCLSID, REFIID, LPVOID*);
                DllGetClassObjectFunc dllGetClassObject = 
                    reinterpret_cast<DllGetClassObjectFunc>(GetProcAddress(hDll, "DllGetClassObject"));
                
                if (dllGetClassObject) {
                    // Call DllGetClassObject
                    HRESULT hr = dllGetClassObject(rclsid, riid, ppv);
                    if (SUCCEEDED(hr)) {
                        return hr;
                    }
                }
                
                // Don't free the library - it needs to stay loaded for the class factory
                // FreeLibrary(hDll);
            }
        }
    }
    
    // Not redirected or failed - use original COM
    return Real_CoGetClassObject(rclsid, dwClsContext, pServerInfo, riid, ppv);
}

bool InstallCOMHooks() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    
    DetourAttach(&(PVOID&)Real_CoCreateInstance, Hooked_CoCreateInstance);
    DetourAttach(&(PVOID&)Real_CoCreateInstanceEx, Hooked_CoCreateInstanceEx);
    DetourAttach(&(PVOID&)Real_CoGetClassObject, Hooked_CoGetClassObject);
    
    LONG error = DetourTransactionCommit();
    return error == NO_ERROR;
}

bool RemoveCOMHooks() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    
    DetourDetach(&(PVOID&)Real_CoCreateInstance, Hooked_CoCreateInstance);
    DetourDetach(&(PVOID&)Real_CoCreateInstanceEx, Hooked_CoCreateInstanceEx);
    DetourDetach(&(PVOID&)Real_CoGetClassObject, Hooked_CoGetClassObject);
    
    LONG error = DetourTransactionCommit();
    return error == NO_ERROR;
}
