// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "RegistryHooks.h"
#include "RegistryConfig.h"
#include "COMHooks.h"
#include "COMConfig.h"
#include <string>

// Global module handle
HMODULE g_hModule = NULL;

// Helper function to get DLL directory path
std::wstring GetDllDirectory() {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(g_hModule, path, MAX_PATH) > 0) {
        std::wstring fullPath(path);
        size_t lastSlash = fullPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            return fullPath.substr(0, lastSlash + 1);
        }
    }
    return L"";
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        {
            g_hModule = hModule;
            
            // Disable thread library calls for performance
            DisableThreadLibraryCalls(hModule);
            
            // Load configuration files from DLL directory
            std::wstring dllDir = GetDllDirectory();
            
            // Load registry configuration from .reg file
            std::wstring regConfigPath = dllDir + L"registry_config.reg";
            if (RegistryConfig::GetInstance().LoadConfiguration(regConfigPath)) {
                if (RegistryConfig::GetInstance().IsEnabled()) {
                    InstallRegistryHooks();
                }
            }
            
            // Load COM configuration from .ini file
            std::wstring comConfigPath = dllDir + L"com_config.ini";
            if (COMConfig::GetInstance().LoadConfiguration(comConfigPath)) {
                if (COMConfig::GetInstance().IsEnabled()) {
                    InstallCOMHooks();
                }
            }
        }
        break;
        
    case DLL_THREAD_ATTACH:
        break;
        
    case DLL_THREAD_DETACH:
        break;
        
    case DLL_PROCESS_DETACH:
        if (lpReserved == nullptr) {
            // Only cleanup if we're not unloading due to process termination
            RemoveCOMHooks();
            RemoveRegistryHooks();
        }
        break;
    }
    return TRUE;
}

