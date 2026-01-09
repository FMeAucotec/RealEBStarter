#include "pch.h"
#include "COMConfig.h"
#include <fstream>
#include <sstream>
#include <algorithm>

COMConfig& COMConfig::GetInstance() {
    static COMConfig instance;
    return instance;
}

COMConfig::~COMConfig() {
    // Unload all DLLs
    for (auto& pair : m_mappings) {
        if (pair.second.isLoaded && pair.second.hModule) {
            FreeLibrary(pair.second.hModule);
        }
    }
}

bool COMConfig::LoadConfiguration(const std::wstring& configFilePath) {
    std::wifstream file(configFilePath);
    if (!file.is_open()) {
        return false;
    }
    
    // Extract base path from config file path
    size_t lastSlash = configFilePath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        m_basePath = configFilePath.substr(0, lastSlash + 1);
    }
    
    m_mappings.clear();
    std::wstring line;
    
    while (std::getline(file, line)) {
        line = Trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == L'#' || line[0] == L';') {
            continue;
        }
        
        // Check for enabled flag
        if (line.find(L"enabled=") == 0) {
            std::wstring value = Trim(line.substr(8));
            std::transform(value.begin(), value.end(), value.begin(), ::towlower);
            m_enabled = (value == L"true" || value == L"1" || value == L"yes");
            continue;
        }
        
        ParseLine(line);
    }
    
    file.close();
    return true;
}

void COMConfig::ParseLine(const std::wstring& line) {
    // Format: "CLSID=DLL_PATH"
    // Example: "{12345678-1234-1234-1234-123456789ABC}=MyComServer.dll"
    
    size_t equalPos = line.find(L'=');
    if (equalPos == std::wstring::npos) {
        return;
    }
    
    std::wstring clsidStr = Trim(line.substr(0, equalPos));
    std::wstring dllPath = Trim(line.substr(equalPos + 1));
    
    // Remove quotes from DLL path if present
    if (!dllPath.empty() && dllPath.front() == L'"' && dllPath.back() == L'"') {
        dllPath = dllPath.substr(1, dllPath.length() - 2);
    }
    
    CLSID clsid;
    if (!ParseCLSID(clsidStr, clsid)) {
        return;  // Invalid CLSID
    }
    
    // Resolve relative path
    std::wstring resolvedPath = ResolvePath(dllPath, m_basePath);
    
    COMDllMapping mapping;
    mapping.clsidString = clsidStr;
    mapping.clsid = clsid;
    mapping.dllPath = resolvedPath;
    mapping.hModule = nullptr;
    mapping.isLoaded = false;
    
    m_mappings[clsid] = mapping;
}

bool COMConfig::ParseCLSID(const std::wstring& str, CLSID& clsid) {
    // Try to parse CLSID from string
    // Format: {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}
    
    HRESULT hr = CLSIDFromString(str.c_str(), &clsid);
    return SUCCEEDED(hr);
}

std::wstring COMConfig::CLSIDToString(REFCLSID clsid) {
    LPOLESTR clsidStr = nullptr;
    if (SUCCEEDED(StringFromCLSID(clsid, &clsidStr))) {
        std::wstring result(clsidStr);
        CoTaskMemFree(clsidStr);
        return result;
    }
    return L"";
}

std::wstring COMConfig::Trim(const std::wstring& str) {
    size_t first = str.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return L"";
    }
    size_t last = str.find_last_not_of(L" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::wstring COMConfig::ResolvePath(const std::wstring& path, const std::wstring& basePath) {
    // Check if path is absolute
    if (path.length() >= 2 && path[1] == L':') {
        // Absolute path with drive letter
        return path;
    }
    
    if (path.length() >= 2 && (path[0] == L'\\' || path[0] == L'/')) {
        // Absolute path starting with backslash
        return path;
    }
    
    // Relative path - combine with base path
    return basePath + path;
}

bool COMConfig::HasRedirectedCLSID(REFCLSID rclsid) {
    if (!m_enabled) {
        return false;
    }
    
    return m_mappings.find(rclsid) != m_mappings.end();
}

bool COMConfig::GetDllPath(REFCLSID rclsid, std::wstring& dllPath) {
    if (!m_enabled) {
        return false;
    }
    
    auto it = m_mappings.find(rclsid);
    if (it == m_mappings.end()) {
        return false;
    }
    
    dllPath = it->second.dllPath;
    return true;
}

bool COMConfig::LoadDllAndGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!m_enabled || !ppv) {
        return false;
    }
    
    auto it = m_mappings.find(rclsid);
    if (it == m_mappings.end()) {
        return false;
    }
    
    COMDllMapping& mapping = it->second;
    
    // Load DLL if not already loaded
    if (!mapping.isLoaded) {
        mapping.hModule = LoadLibraryW(mapping.dllPath.c_str());
        if (!mapping.hModule) {
            return false;
        }
        mapping.isLoaded = true;
    }
    
    // Get DllGetClassObject function
    DllGetClassObjectFunc dllGetClassObject = 
        reinterpret_cast<DllGetClassObjectFunc>(GetProcAddress(mapping.hModule, "DllGetClassObject"));
    
    if (!dllGetClassObject) {
        return false;
    }
    
    // Get the class factory
    IClassFactory* pFactory = nullptr;
    HRESULT hr = dllGetClassObject(rclsid, IID_IClassFactory, reinterpret_cast<LPVOID*>(&pFactory));
    
    if (FAILED(hr) || !pFactory) {
        return false;
    }
    
    // Create the instance
    hr = pFactory->CreateInstance(nullptr, riid, ppv);
    pFactory->Release();
    
    return SUCCEEDED(hr);
}
