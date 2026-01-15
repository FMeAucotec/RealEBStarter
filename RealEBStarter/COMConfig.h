#pragma once
#include <string>
#include <map>
#include <Windows.h>
#include <combaseapi.h>
#include <vcclr.h>

// Custom comparer for CLSID to use in map
struct CLSIDComparer {
    bool operator()(const CLSID& a, const CLSID& b) const {
        return memcmp(&a, &b, sizeof(CLSID)) < 0;
    }
};

// Enum for action type
enum class COMActionType {
    LoadDLL,        // Load a COM DLL
    ShellExecute    // Execute a shell command
};

// Structure to hold COM DLL information
struct COMDllMapping {
    std::wstring clsidString;     // String representation of CLSID
    CLSID clsid;                  // Parsed CLSID
    COMActionType actionType;     // Type of action to perform
    std::wstring dllPath;         // Path to DLL (absolute or relative)
    std::wstring shellCommand;    // Shell command to execute (for ShellExecute)
    std::wstring shellParameters; // Command line parameters (for ShellExecute)
    std::wstring shellVerb;       // Shell verb (open, runas, etc.)
    HMODULE hModule;              // Loaded module handle
    bool isLoaded;                // Whether DLL is loaded
    bool isDotNet = false;
    bool shellExecuted;           // Whether shell command has been executed
    gcroot<System::Reflection::Assembly^> managedAssembly = nullptr; // Managed assembly reference (if applicable)
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
    
    // Execute shell command for a CLSID
    bool ExecuteShellCommand(REFCLSID rclsid);
    
    // Check if CLSID is mapped to a shell command
    bool IsShellExecuteAction(REFCLSID rclsid);
    
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
    std::wstring RemoveQuotes(const std::wstring& str);
    std::wstring ExtractQuotedString(const std::wstring& str, size_t& pos);
    bool ParseCLSID(const std::wstring& str, CLSID& clsid);
    std::wstring CLSIDToString(REFCLSID clsid);
    
    // DLL loading
    typedef HRESULT (STDAPICALLTYPE *DllGetClassObjectFunc)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
};
