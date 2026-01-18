#pragma once
#include <Windows.h>

// Function prototypes for original API functions
typedef LSTATUS (WINAPI* RegOpenKeyExW_t)(
    HKEY hKey,
    LPCWSTR lpSubKey,
    DWORD ulOptions,
    REGSAM samDesired,
    PHKEY phkResult
);

typedef LSTATUS (WINAPI* RegOpenKeyExA_t)(
    HKEY hKey,
    LPCSTR lpSubKey,
    DWORD ulOptions,
    REGSAM samDesired,
    PHKEY phkResult
);

typedef LSTATUS (WINAPI* RegQueryValueExW_t)(
    HKEY hKey,
    LPCWSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData
);

typedef LSTATUS (WINAPI* RegQueryValueExA_t)(
    HKEY hKey,
    LPCSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData
);

typedef LSTATUS (WINAPI* RegSetValueExW_t)(
    HKEY hKey,
    LPCWSTR lpValueName,
    DWORD Reserved,
    DWORD dwType,
    const BYTE* lpData,
    DWORD cbData
);

typedef LSTATUS (WINAPI* RegSetValueExA_t)(
    HKEY hKey,
    LPCSTR lpValueName,
    DWORD Reserved,
    DWORD dwType,
    const BYTE* lpData,
    DWORD cbData
);

typedef LSTATUS (WINAPI* RegCreateKeyExW_t)(
    HKEY hKey,
    LPCWSTR lpSubKey,
    DWORD Reserved,
    LPWSTR lpClass,
    DWORD dwOptions,
    REGSAM samDesired,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PHKEY phkResult,
    LPDWORD lpdwDisposition
);

typedef LSTATUS (WINAPI* RegCreateKeyExA_t)(
    HKEY hKey,
    LPCSTR lpSubKey,
    DWORD Reserved,
    LPSTR lpClass,
    DWORD dwOptions,
    REGSAM samDesired,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PHKEY phkResult,
    LPDWORD lpdwDisposition
);

typedef LSTATUS (WINAPI* RegDeleteValueW_t)(
    HKEY hKey,
    LPCWSTR lpValueName
);

typedef LSTATUS (WINAPI* RegDeleteValueA_t)(
    HKEY hKey,
    LPCSTR lpValueName
);

typedef LSTATUS (WINAPI* RegGetValueW_t)(
    HKEY hKey,
    LPCWSTR lpSubKey,
    LPCWSTR lpValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pvData,
    LPDWORD pcbData
);

typedef LSTATUS (WINAPI* RegGetValueA_t)(
    HKEY hKey,
    LPCSTR lpSubKey,
    LPCSTR lpValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pvData,
    LPDWORD pcbData
);

typedef LSTATUS (WINAPI* RegCloseKey_t)(
    HKEY hKey
);

// Hooked functions
LSTATUS WINAPI Hooked_RegOpenKeyExW(
    HKEY hKey,
    LPCWSTR lpSubKey,
    DWORD ulOptions,
    REGSAM samDesired,
    PHKEY phkResult
);

LSTATUS WINAPI Hooked_RegOpenKeyExA(
    HKEY hKey,
    LPCSTR lpSubKey,
    DWORD ulOptions,
    REGSAM samDesired,
    PHKEY phkResult
);

LSTATUS WINAPI Hooked_RegQueryValueExW(
    HKEY hKey,
    LPCWSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData
);

LSTATUS WINAPI Hooked_RegQueryValueExA(
    HKEY hKey,
    LPCSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData
);

LSTATUS WINAPI Hooked_RegSetValueExW(
    HKEY hKey,
    LPCWSTR lpValueName,
    DWORD Reserved,
    DWORD dwType,
    const BYTE* lpData,
    DWORD cbData
);

LSTATUS WINAPI Hooked_RegSetValueExA(
    HKEY hKey,
    LPCSTR lpValueName,
    DWORD Reserved,
    DWORD dwType,
    const BYTE* lpData,
    DWORD cbData
);

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
);

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
);

LSTATUS WINAPI Hooked_RegDeleteValueW(
    HKEY hKey,
    LPCWSTR lpValueName
);

LSTATUS WINAPI Hooked_RegDeleteValueA(
    HKEY hKey,
    LPCSTR lpValueName
);

LSTATUS WINAPI Hooked_RegGetValueW(
    HKEY hKey,
    LPCWSTR lpSubKey,
    LPCWSTR lpValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pvData,
    LPDWORD pcbData
);

LSTATUS WINAPI Hooked_RegGetValueA(
    HKEY hKey,
    LPCSTR lpSubKey,
    LPCSTR lpValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pvData,
    LPDWORD pcbData
);

LSTATUS WINAPI Hooked_RegCloseKey(
    HKEY hKey
);

// Hook installation/removal
bool InstallRegistryHooks();
bool RemoveRegistryHooks();

// Original function pointers
extern RegOpenKeyExW_t Real_RegOpenKeyExW;
extern RegOpenKeyExA_t Real_RegOpenKeyExA;
extern RegQueryValueExW_t Real_RegQueryValueExW;
extern RegQueryValueExA_t Real_RegQueryValueExA;
extern RegSetValueExW_t Real_RegSetValueExW;
extern RegSetValueExA_t Real_RegSetValueExA;
extern RegCreateKeyExW_t Real_RegCreateKeyExW;
extern RegCreateKeyExA_t Real_RegCreateKeyExA;
extern RegDeleteValueW_t Real_RegDeleteValueW;
extern RegDeleteValueA_t Real_RegDeleteValueA;
extern RegGetValueW_t Real_RegGetValueW;
extern RegGetValueA_t Real_RegGetValueA;
extern RegCloseKey_t Real_RegCloseKey;
