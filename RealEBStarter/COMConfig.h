#pragma once
#include <string>
#include <map>
#include <Windows.h>
#include <combaseapi.h>

// Custom comparer for CLSID to use in map
struct CLSIDComparer {
    bool operator()(const CLSID& a, const CLSID& b) const {
        return memcmp(&a, &b, sizeof(CLSID)) < 0;
    }
};

// Structure to hold COM DLL information
struct COMDllMapping {
    std::wstring clsidString;  // String representation of CLSID
    CLSID clsid;              // Parsed CLSID
    std::wstring dllPath;     // Path to DLL (absolute or relative)
    HMODULE hModule;          // Loaded module handle
    bool isLoaded;            // Whether DLL is loaded
};

class COMConfig {
public:
    static COMConfig& GetInstance();
    
    bool LoadConfiguration(const std::wstring& configFilePath);
    
    // Check if a CLSID should be redirected
    bool HasRedirectedCLSID(REFCLSID rclsid);
    
    // Get the DLL path for a redirected CLSID
    bool GetDllPath(REFCLSID rclsid, std::wstring& dllPath);
    
    // Load the DLL and get DllGetClassObject function
    bool LoadDllAndGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
    
    bool IsEnabled() const { return m_enabled; }
    const std::map<CLSID, COMDllMapping, CLSIDComparer>& GetMappings() const { return m_mappings; }
    
    // Helper to resolve relative paths
    std::wstring ResolvePath(const std::wstring& path, const std::wstring& basePath);
    
private:
    COMConfig() : m_enabled(true) {}
    ~COMConfig();
    
    COMConfig(const COMConfig&) = delete;
    COMConfig& operator=(const COMConfig&) = delete;
    
    std::map<CLSID, COMDllMapping, ::CLSIDComparer> m_mappings;
    std::wstring m_basePath;  // Base path for relative paths
    bool m_enabled;
    
    // Parsing helpers
    void ParseLine(const std::wstring& line);
    std::wstring Trim(const std::wstring& str);
    bool ParseCLSID(const std::wstring& str, CLSID& clsid);
    std::wstring CLSIDToString(REFCLSID clsid);
    
    // DLL loading
    typedef HRESULT (STDAPICALLTYPE *DllGetClassObjectFunc)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
};
