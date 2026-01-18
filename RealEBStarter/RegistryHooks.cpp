#include "pch.h"
#include "RegistryHooks.h"
#include "RegistryConfig.h"
#include <string>
#include <map>
#include <mutex>

// MS Detours - ensure you have installed it in $(SolutionDir)Detours
#pragma comment(lib, "detours.lib")
extern "C" {
    LONG WINAPI DetourTransactionBegin();
    LONG WINAPI DetourUpdateThread(HANDLE hThread);
    LONG WINAPI DetourAttach(PVOID* ppPointer, PVOID pDetour);
    LONG WINAPI DetourDetach(PVOID* ppPointer, PVOID pDetour);
    LONG WINAPI DetourTransactionCommit();
}

// Original function pointers
RegOpenKeyExW_t Real_RegOpenKeyExW = RegOpenKeyExW;
RegOpenKeyExA_t Real_RegOpenKeyExA = RegOpenKeyExA;
RegQueryValueExW_t Real_RegQueryValueExW = RegQueryValueExW;
RegQueryValueExA_t Real_RegQueryValueExA = RegQueryValueExA;
RegSetValueExW_t Real_RegSetValueExW = RegSetValueExW;
RegSetValueExA_t Real_RegSetValueExA = RegSetValueExA;
RegCreateKeyExW_t Real_RegCreateKeyExW = RegCreateKeyExW;
RegCreateKeyExA_t Real_RegCreateKeyExA = RegCreateKeyExA;
RegDeleteValueW_t Real_RegDeleteValueW = RegDeleteValueW;
RegDeleteValueA_t Real_RegDeleteValueA = RegDeleteValueA;
RegGetValueW_t Real_RegGetValueW = RegGetValueW;
RegGetValueA_t Real_RegGetValueA = RegGetValueA;
RegCloseKey_t Real_RegCloseKey = RegCloseKey;

// Map to track key handles and their paths
static std::map<HKEY, std::wstring> g_keyHandleMap;
static std::mutex g_keyHandleMutex;

bool IsFakeKeyHandle(HKEY hKey) {
    return (reinterpret_cast<uintptr_t>(hKey) & 0xFFFF0000) == 0xFABE0000;
}

// Helper function to get full registry path from HKEY
std::wstring GetKeyPath(HKEY hKey, LPCWSTR lpSubKey) {
    std::wstring rootKey;
    
    if (hKey == HKEY_CLASSES_ROOT) rootKey = L"HKEY_CLASSES_ROOT";
    else if (hKey == HKEY_CURRENT_USER) rootKey = L"HKEY_CURRENT_USER";
    else if (hKey == HKEY_LOCAL_MACHINE) rootKey = L"HKEY_LOCAL_MACHINE";
    else if (hKey == HKEY_USERS) rootKey = L"HKEY_USERS";
    else if (hKey == HKEY_CURRENT_CONFIG) rootKey = L"HKEY_CURRENT_CONFIG";
    else {
        // Check if this is a tracked handle
        std::lock_guard<std::mutex> lock(g_keyHandleMutex);
        auto it = g_keyHandleMap.find(hKey);
        if (it != g_keyHandleMap.end()) {
            rootKey = it->second;
        }
    }
    
    if (lpSubKey && lpSubKey[0] != L'\0') {
        if (!rootKey.empty()) {
            return rootKey + L"\\" + lpSubKey;
        }
        return lpSubKey;
    }
    
    return rootKey;
}

// Convert ANSI to Wide string
std::wstring AnsiToWide(LPCSTR str) {
    if (!str) return L"";
    int size = MultiByteToWideChar(CP_ACP, 0, str, -1, nullptr, 0);
    if (size <= 0) return L"";
    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, str, -1, &result[0], size);
    return result;
}

// Hooked RegOpenKeyExW
LSTATUS WINAPI Hooked_RegOpenKeyExW(
    HKEY hKey,
    LPCWSTR lpSubKey,
    DWORD ulOptions,
    REGSAM samDesired,
    PHKEY phkResult
) {
    std::wstring keyPath = GetKeyPath(hKey, lpSubKey);
    
    // Check if this key is in our virtual registry
    if (RegistryConfig::GetInstance().HasRedirectedKey(keyPath)) {
        // Create a fake handle for this virtual key
        HKEY fakeHandle = (HKEY)(0xFABE0000 | (g_keyHandleMap.size() + 1));
        {
            std::lock_guard<std::mutex> lock(g_keyHandleMutex);
            g_keyHandleMap[fakeHandle] = keyPath;
        }
        
        if (phkResult) {
            *phkResult = fakeHandle;
        }
        return ERROR_SUCCESS;
    }
    
    // Not in virtual registry, pass through to real registry
    LSTATUS result = Real_RegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
    if (result == ERROR_SUCCESS && phkResult) {
        std::lock_guard<std::mutex> lock(g_keyHandleMutex);
        g_keyHandleMap[*phkResult] = keyPath;
    }
    return result;
}

// Hooked RegOpenKeyExA
LSTATUS WINAPI Hooked_RegOpenKeyExA(
    HKEY hKey,
    LPCSTR lpSubKey,
    DWORD ulOptions,
    REGSAM samDesired,
    PHKEY phkResult
) {
    std::wstring wideSubKey = AnsiToWide(lpSubKey);
    return Hooked_RegOpenKeyExW(hKey, wideSubKey.c_str(), ulOptions, samDesired, phkResult);
}

// Hooked RegQueryValueExW
LSTATUS WINAPI Hooked_RegQueryValueExW(
    HKEY hKey,
    LPCWSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData
) {
    std::wstring keyPath;
    {
        std::lock_guard<std::mutex> lock(g_keyHandleMutex);
        auto it = g_keyHandleMap.find(hKey);
        if (it != g_keyHandleMap.end()) {
            keyPath = it->second;
        }
    }
    
    if (!keyPath.empty()) {
        std::wstring valueName = lpValueName ? lpValueName : L"";
        DWORD type = 0;
        DWORD dataSize = lpcbData ? *lpcbData : 0;
        
        // Check if this value is in our virtual registry
        if (RegistryConfig::GetInstance().GetRedirectedValue(keyPath, valueName, &type, lpData, &dataSize)) {
            if (lpType) {
                *lpType = type;
            }
            if (lpcbData) {
                *lpcbData = dataSize;
            }
            
            // If buffer is too small, return ERROR_MORE_DATA
            if (lpData == nullptr || (lpcbData && *lpcbData < dataSize)) {
                if (lpcbData) {
                    *lpcbData = dataSize;
                }
                return ERROR_MORE_DATA;
            }
            
            return ERROR_SUCCESS;
        }
    }
    
    // Not in virtual registry, pass through to real registry
    return Real_RegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

// Hooked RegQueryValueExA
LSTATUS WINAPI Hooked_RegQueryValueExA(
    HKEY hKey,
    LPCSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData
) {
    std::wstring wideValueName = AnsiToWide(lpValueName);
    return Hooked_RegQueryValueExW(hKey, wideValueName.c_str(), lpReserved, lpType, lpData, lpcbData);
}

// Hooked RegGetValueW
LSTATUS WINAPI Hooked_RegGetValueW(
    HKEY hKey,
    LPCWSTR lpSubKey,
    LPCWSTR lpValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pvData,
    LPDWORD pcbData
) {
    std::wstring keyPath = GetKeyPath(hKey, lpSubKey);
    if (!keyPath.empty() && RegistryConfig::GetInstance().IsUnderRedirectedKey(keyPath)) {
        std::wstring valueName = lpValue ? lpValue : L"";
        DWORD type = 0;
        DWORD dataSize = pcbData ? *pcbData : 0;

        if (RegistryConfig::GetInstance().GetRedirectedValue(keyPath, valueName, &type,
                                                             static_cast<BYTE*>(pvData), &dataSize)) {
            if (pdwType) {
                *pdwType = type;
            }
            if (pcbData) {
                *pcbData = dataSize;
            }

            if (pvData == nullptr || (pcbData && *pcbData < dataSize)) {
                if (pcbData) {
                    *pcbData = dataSize;
                }
                return ERROR_MORE_DATA;
            }

            return ERROR_SUCCESS;
        }

        return ERROR_FILE_NOT_FOUND;
    }

    return Real_RegGetValueW(hKey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

// Hooked RegGetValueA
LSTATUS WINAPI Hooked_RegGetValueA(
    HKEY hKey,
    LPCSTR lpSubKey,
    LPCSTR lpValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pvData,
    LPDWORD pcbData
) {
    std::wstring wideSubKey = AnsiToWide(lpSubKey);
    std::wstring wideValue = AnsiToWide(lpValue);
    return Hooked_RegGetValueW(hKey, wideSubKey.c_str(), wideValue.c_str(), dwFlags, pdwType, pvData, pcbData);
}

// Hooked RegCloseKey
LSTATUS WINAPI Hooked_RegCloseKey(
    HKEY hKey
) {
    if (IsFakeKeyHandle(hKey)) {
        std::lock_guard<std::mutex> lock(g_keyHandleMutex);
        g_keyHandleMap.erase(hKey);
        return ERROR_SUCCESS;
    }

    return Real_RegCloseKey(hKey);
}

// Hooked RegSetValueExW
LSTATUS WINAPI Hooked_RegSetValueExW(
    HKEY hKey,
    LPCWSTR lpValueName,
    DWORD Reserved,
    DWORD dwType,
    const BYTE* lpData,
    DWORD cbData
) {
    std::wstring keyPath;
    {
        std::lock_guard<std::mutex> lock(g_keyHandleMutex);
        auto it = g_keyHandleMap.find(hKey);
        if (it != g_keyHandleMap.end()) {
            keyPath = it->second;
        }
    }
    
    // If this is a virtual registry key, write into the virtual store
    RegistryConfig& registryConfig = RegistryConfig::GetInstance();
    if (!keyPath.empty() && registryConfig.IsUnderRedirectedKey(keyPath)) {
        if (!registryConfig.SetRedirectedValue(keyPath, lpValueName ? lpValueName : L"",
                                              dwType, lpData, cbData)) {
            return ERROR_INVALID_PARAMETER;
        }
        return ERROR_SUCCESS;
    }
    
    return Real_RegSetValueExW(hKey, lpValueName, Reserved, dwType, lpData, cbData);
}

// Hooked RegSetValueExA
LSTATUS WINAPI Hooked_RegSetValueExA(
    HKEY hKey,
    LPCSTR lpValueName,
    DWORD Reserved,
    DWORD dwType,
    const BYTE* lpData,
    DWORD cbData
) {
    std::wstring wideValueName = AnsiToWide(lpValueName);
    return Hooked_RegSetValueExW(hKey, wideValueName.c_str(), Reserved, dwType, lpData, cbData);
}

// Hooked RegCreateKeyExW
LSTATUS WINAPI Hooked_RegCreateKeyExW(
    HKEY hKey,
    LPCWSTR lpSubKey,
    DWORD Reserved,
    LPWSTR lpClass,
    DWORD dwOptions,
    REGSAM samDesired,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PHKEY phkResult,
    LPDWORD lpdwDisposition
) {
    std::wstring keyPath = GetKeyPath(hKey, lpSubKey);
    
    RegistryConfig& registryConfig = RegistryConfig::GetInstance();

    // If this key is in the virtual registry, create/return a fake handle and allow writes.
    const bool existed = registryConfig.IsUnderRedirectedKey(keyPath);
    if (existed || registryConfig.EnsureRedirectedKey(keyPath)) {
        HKEY fakeHandle = (HKEY)(0xFABE0000 | (g_keyHandleMap.size() + 1));
        {
            std::lock_guard<std::mutex> lock(g_keyHandleMutex);
            g_keyHandleMap[fakeHandle] = keyPath;
        }

        if (phkResult) {
            *phkResult = fakeHandle;
        }
        if (lpdwDisposition) {
            *lpdwDisposition = existed ? REG_OPENED_EXISTING_KEY : REG_CREATED_NEW_KEY;
        }
        return ERROR_SUCCESS;
    }

    // Not in virtual registry, pass through to real registry
    LSTATUS result = Real_RegCreateKeyExW(hKey, lpSubKey, Reserved, lpClass, dwOptions, 
                                          samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
    if (result == ERROR_SUCCESS && phkResult) {
        std::lock_guard<std::mutex> lock(g_keyHandleMutex);
        g_keyHandleMap[*phkResult] = keyPath;
    }
    return result;
}

// Hooked RegCreateKeyExA
LSTATUS WINAPI Hooked_RegCreateKeyExA(
    HKEY hKey,
    LPCSTR lpSubKey,
    DWORD Reserved,
    LPSTR lpClass,
    DWORD dwOptions,
    REGSAM samDesired,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PHKEY phkResult,
    LPDWORD lpdwDisposition
) {
    std::wstring wideSubKey = AnsiToWide(lpSubKey);
    std::wstring wideClass = AnsiToWide(lpClass);
    return Hooked_RegCreateKeyExW(hKey, wideSubKey.c_str(), Reserved, 
                                  const_cast<LPWSTR>(wideClass.c_str()), 
                                  dwOptions, samDesired, lpSecurityAttributes, 
                                  phkResult, lpdwDisposition);
}

// Hooked RegDeleteValueW
LSTATUS WINAPI Hooked_RegDeleteValueW(
    HKEY hKey,
    LPCWSTR lpValueName
) {
    std::wstring keyPath;
    {
        std::lock_guard<std::mutex> lock(g_keyHandleMutex);
        auto it = g_keyHandleMap.find(hKey);
        if (it != g_keyHandleMap.end()) {
            keyPath = it->second;
        }
    }
    
    // If this is a virtual registry key, block deletes (read-only)
    if (!keyPath.empty() && RegistryConfig::GetInstance().HasRedirectedKey(keyPath)) {
        return ERROR_ACCESS_DENIED;
    }
    
    return Real_RegDeleteValueW(hKey, lpValueName);
}

// Hooked RegDeleteValueA
LSTATUS WINAPI Hooked_RegDeleteValueA(
    HKEY hKey,
    LPCSTR lpValueName
) {
    std::wstring wideValueName = AnsiToWide(lpValueName);
    return Hooked_RegDeleteValueW(hKey, wideValueName.c_str());
}

bool InstallRegistryHooks() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    
    DetourAttach(&(PVOID&)Real_RegOpenKeyExW, Hooked_RegOpenKeyExW);
    DetourAttach(&(PVOID&)Real_RegOpenKeyExA, Hooked_RegOpenKeyExA);
    DetourAttach(&(PVOID&)Real_RegQueryValueExW, Hooked_RegQueryValueExW);
    DetourAttach(&(PVOID&)Real_RegQueryValueExA, Hooked_RegQueryValueExA);
    DetourAttach(&(PVOID&)Real_RegSetValueExW, Hooked_RegSetValueExW);
    DetourAttach(&(PVOID&)Real_RegSetValueExA, Hooked_RegSetValueExA);
    DetourAttach(&(PVOID&)Real_RegCreateKeyExW, Hooked_RegCreateKeyExW);
    DetourAttach(&(PVOID&)Real_RegCreateKeyExA, Hooked_RegCreateKeyExA);
    DetourAttach(&(PVOID&)Real_RegDeleteValueW, Hooked_RegDeleteValueW);
    DetourAttach(&(PVOID&)Real_RegDeleteValueA, Hooked_RegDeleteValueA);
    DetourAttach(&(PVOID&)Real_RegGetValueW, Hooked_RegGetValueW);
    DetourAttach(&(PVOID&)Real_RegGetValueA, Hooked_RegGetValueA);
    DetourAttach(&(PVOID&)Real_RegCloseKey, Hooked_RegCloseKey);
    
    LONG error = DetourTransactionCommit();
    return error == NO_ERROR;
}

bool RemoveRegistryHooks() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    
    DetourDetach(&(PVOID&)Real_RegOpenKeyExW, Hooked_RegOpenKeyExW);
    DetourDetach(&(PVOID&)Real_RegOpenKeyExA, Hooked_RegOpenKeyExA);
    DetourDetach(&(PVOID&)Real_RegQueryValueExW, Hooked_RegQueryValueExW);
    DetourDetach(&(PVOID&)Real_RegQueryValueExA, Hooked_RegQueryValueExA);
    DetourDetach(&(PVOID&)Real_RegSetValueExW, Hooked_RegSetValueExW);
    DetourDetach(&(PVOID&)Real_RegSetValueExA, Hooked_RegSetValueExA);
    DetourDetach(&(PVOID&)Real_RegCreateKeyExW, Hooked_RegCreateKeyExW);
    DetourDetach(&(PVOID&)Real_RegCreateKeyExA, Hooked_RegCreateKeyExA);
    DetourDetach(&(PVOID&)Real_RegDeleteValueW, Hooked_RegDeleteValueW);
    DetourDetach(&(PVOID&)Real_RegDeleteValueA, Hooked_RegDeleteValueA);
    DetourDetach(&(PVOID&)Real_RegGetValueW, Hooked_RegGetValueW);
    DetourDetach(&(PVOID&)Real_RegGetValueA, Hooked_RegGetValueA);
    DetourDetach(&(PVOID&)Real_RegCloseKey, Hooked_RegCloseKey);
    
    LONG error = DetourTransactionCommit();
    return error == NO_ERROR;
}
