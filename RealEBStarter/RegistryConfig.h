#pragma once
#include <string>
#include <map>
#include <vector>
#include <Windows.h>

// Structure to hold registry value data
struct RegistryValue {
    std::wstring valueName;
    DWORD type;
    std::vector<BYTE> data;
};

// Structure to hold a complete registry key with all its values
struct RegistryKey {
    std::wstring keyPath;
    std::map<std::wstring, RegistryValue> values;
};

class RegistryConfig {
public:
    static RegistryConfig& GetInstance();
    
    bool LoadConfiguration(const std::wstring& regFilePath);
    
    // Check if a registry read should be intercepted
    bool GetRedirectedValue(const std::wstring& keyPath, const std::wstring& valueName,
                           DWORD* outType, BYTE* outData, DWORD* outDataSize);
    
    // Check if a key exists in redirected registry
    bool HasRedirectedKey(const std::wstring& keyPath);
    
    // Get all value names for a redirected key
    bool GetRedirectedValueNames(const std::wstring& keyPath, std::vector<std::wstring>& valueNames);
    
    bool IsEnabled() const { return m_enabled; }
    const std::map<std::wstring, RegistryKey>& GetKeys() const { return m_keys; }
    
private:
    RegistryConfig() : m_enabled(true) {}
    ~RegistryConfig() = default;
    
    RegistryConfig(const RegistryConfig&) = delete;
    RegistryConfig& operator=(const RegistryConfig&) = delete;
    
    std::map<std::wstring, RegistryKey> m_keys;  // Case-insensitive key lookup
    bool m_enabled;
    
    // Parsing helpers
    bool ParseRegFile(const std::wstring& content);
    std::wstring ReadFileContent(const std::wstring& filePath);
    std::wstring Trim(const std::wstring& str);
    std::wstring RemoveQuotes(const std::wstring& str);
    std::wstring NormalizeKeyPath(const std::wstring& keyPath);
    bool ParseValueLine(const std::wstring& line, std::wstring& valueName, DWORD& type, std::vector<BYTE>& data);
    std::vector<BYTE> ParseHexData(const std::wstring& hexStr, DWORD type);
    std::vector<BYTE> ParseStringData(const std::wstring& str);
    DWORD ParseDwordData(const std::wstring& str);
};
