#include "pch.h"
#include "RegistryConfig.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

RegistryConfig& RegistryConfig::GetInstance() {
    static RegistryConfig instance;
    return instance;
}

std::wstring RegistryConfig::ReadFileContent(const std::wstring& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return L"";
    }
    
    // Read file content
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    // Check for UTF-16 LE BOM (FF FE)
    if (content.size() >= 2 && (unsigned char)content[0] == 0xFF && (unsigned char)content[1] == 0xFE) {
        // UTF-16 LE encoded - convert to wstring
        const wchar_t* wdata = reinterpret_cast<const wchar_t*>(content.data() + 2);
        size_t wlen = (content.size() - 2) / sizeof(wchar_t);
        return std::wstring(wdata, wlen);
    }
    
    // ANSI encoded (REGEDIT4 format) - convert to wstring
    std::wstring result;
    result.reserve(content.size());
    for (char c : content) {
        result += static_cast<wchar_t>(static_cast<unsigned char>(c));
    }
    return result;
}

bool RegistryConfig::LoadConfiguration(const std::wstring& regFilePath) {
    std::wstring content = ReadFileContent(regFilePath);
    if (content.empty()) {
        return false;
    }
    
    m_keys.clear();
    m_enabled = true;
    
    return ParseRegFile(content);
}

bool RegistryConfig::ParseRegFile(const std::wstring& content) {
    std::wistringstream stream(content);
    std::wstring line;
    std::wstring currentKey;
    bool inKey = false;
    
    // Read first line to check header
    if (!std::getline(stream, line)) {
        return false;
    }
    
    line = Trim(line);
    if (line != L"Windows Registry Editor Version 5.00" && line != L"REGEDIT4") {
        return false;  // Invalid .reg file format
    }
    
    // Parse the rest of the file
    std::wstring continuedLine;
    while (std::getline(stream, line)) {
        // Handle line continuations (lines ending with backslash)
        if (!line.empty() && line.back() == L'\\') {
            continuedLine += line.substr(0, line.length() - 1);
            continue;
        }
        
        if (!continuedLine.empty()) {
            line = continuedLine + line;
            continuedLine.clear();
        }
        
        line = Trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == L';') {
            continue;
        }
        
        // Check for key line [HKEY_...]
        if (line[0] == L'[' && line.back() == L']') {
            currentKey = line.substr(1, line.length() - 2);
            currentKey = NormalizeKeyPath(currentKey);
            inKey = true;
            
            // Create key if it doesn't exist
            if (m_keys.find(currentKey) == m_keys.end()) {
                RegistryKey regKey;
                regKey.keyPath = currentKey;
                m_keys[currentKey] = regKey;
            }
            continue;
        }
        
        // Parse value line
        if (inKey && !currentKey.empty()) {
            std::wstring valueName;
            DWORD type;
            std::vector<BYTE> data;
            
            if (ParseValueLine(line, valueName, type, data)) {
                RegistryValue regValue;
                regValue.valueName = valueName;
                regValue.type = type;
                regValue.data = data;
                
                m_keys[currentKey].values[valueName] = regValue;
            }
        }
    }
    
    return !m_keys.empty();
}

bool RegistryConfig::ParseValueLine(const std::wstring& line, std::wstring& valueName, 
                                    DWORD& type, std::vector<BYTE>& data) {
    // Format: "ValueName"=type:data
    // Examples:
    // "StringValue"="String Data"
    // "DwordValue"=dword:00000001
    // "BinaryValue"=hex:01,02,03,04
    // "MultiSz"=hex(7):...
    // @="Default value"
    
    size_t equalPos = line.find(L'=');
    if (equalPos == std::wstring::npos) {
        return false;
    }
    
    std::wstring left = Trim(line.substr(0, equalPos));
    std::wstring right = Trim(line.substr(equalPos + 1));
    
    // Parse value name
    if (left == L"@") {
        valueName = L"";  // Default value
    } else {
        valueName = RemoveQuotes(left);
    }
    
    // Parse value data and type
    if (right[0] == L'"') {
        // String value (REG_SZ)
        type = REG_SZ;
        data = ParseStringData(RemoveQuotes(right));
    } else if (right.find(L"dword:") == 0) {
        // DWORD value
        type = REG_DWORD;
        DWORD dwordValue = ParseDwordData(right.substr(6));
        data.resize(sizeof(DWORD));
        memcpy(data.data(), &dwordValue, sizeof(DWORD));
    } else if (right.find(L"hex:") == 0) {
        // Binary value (REG_BINARY)
        type = REG_BINARY;
        data = ParseHexData(right.substr(4), type);
    } else if (right.find(L"hex(") == 0) {
        // Typed hex value
        size_t closePos = right.find(L')');
        if (closePos != std::wstring::npos) {
            std::wstring typeStr = right.substr(4, closePos - 4);
            type = std::stoul(typeStr, nullptr, 16);
            
            size_t colonPos = right.find(L':', closePos);
            if (colonPos != std::wstring::npos) {
                data = ParseHexData(right.substr(colonPos + 1), type);
            }
        }
    } else {
        return false;
    }
    
    return true;
}

std::vector<BYTE> RegistryConfig::ParseHexData(const std::wstring& hexStr, DWORD type) {
    std::vector<BYTE> result;
    std::wstring cleanStr = Trim(hexStr);
    
    size_t i = 0;
    while (i < cleanStr.length()) {
        // Skip whitespace and commas
        while (i < cleanStr.length() && (cleanStr[i] == L',' || cleanStr[i] == L' ' || 
               cleanStr[i] == L'\t' || cleanStr[i] == L'\r' || cleanStr[i] == L'\n')) {
            i++;
        }
        
        if (i >= cleanStr.length()) break;
        
        // Read two hex digits
        if (i + 1 < cleanStr.length()) {
            std::wstring byteStr = cleanStr.substr(i, 2);
            BYTE byteValue = static_cast<BYTE>(std::stoul(byteStr, nullptr, 16));
            result.push_back(byteValue);
            i += 2;
        } else {
            break;
        }
    }
    
    return result;
}

std::vector<BYTE> RegistryConfig::ParseStringData(const std::wstring& str) {
    std::vector<BYTE> result;
    
    // Convert wstring to bytes including null terminator
    size_t byteCount = (str.length() + 1) * sizeof(wchar_t);
    result.resize(byteCount);
    
    memcpy(result.data(), str.c_str(), byteCount);
    
    return result;
}

DWORD RegistryConfig::ParseDwordData(const std::wstring& str) {
    std::wstring cleanStr = Trim(str);
    return std::stoul(cleanStr, nullptr, 16);
}

std::wstring RegistryConfig::Trim(const std::wstring& str) {
    size_t first = str.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return L"";
    }
    size_t last = str.find_last_not_of(L" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::wstring RegistryConfig::RemoveQuotes(const std::wstring& str) {
    if (str.length() >= 2 && str.front() == L'"' && str.back() == L'"') {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

std::wstring RegistryConfig::NormalizeKeyPath(const std::wstring& keyPath) {
    // Convert to uppercase for case-insensitive comparison
    std::wstring result = keyPath;
    std::transform(result.begin(), result.end(), result.begin(), ::towupper);
    return result;
}

bool RegistryConfig::GetRedirectedValue(const std::wstring& keyPath, const std::wstring& valueName,
                                       DWORD* outType, BYTE* outData, DWORD* outDataSize) {
    if (!m_enabled) {
        return false;
    }
    
    std::wstring normalizedKey = NormalizeKeyPath(keyPath);
    std::wstring normalizedValue = valueName;
    std::transform(normalizedValue.begin(), normalizedValue.end(), normalizedValue.begin(), ::towupper);
    
    auto keyIt = m_keys.find(normalizedKey);
    if (keyIt == m_keys.end()) {
        return false;
    }
    
    auto valueIt = keyIt->second.values.find(normalizedValue);
    if (valueIt == keyIt->second.values.end()) {
        return false;
    }
    
    const RegistryValue& regValue = valueIt->second;
    
    if (outType) {
        *outType = regValue.type;
    }
    
    if (outDataSize) {
        DWORD requiredSize = static_cast<DWORD>(regValue.data.size());
        
        if (outData && *outDataSize >= requiredSize) {
            memcpy(outData, regValue.data.data(), requiredSize);
        }
        
        *outDataSize = requiredSize;
    }
    
    return true;
}

bool RegistryConfig::HasRedirectedKey(const std::wstring& keyPath) {
    if (!m_enabled) {
        return false;
    }
    
    std::wstring normalizedKey = NormalizeKeyPath(keyPath);
    return m_keys.find(normalizedKey) != m_keys.end();
}

bool RegistryConfig::GetRedirectedValueNames(const std::wstring& keyPath, 
                                            std::vector<std::wstring>& valueNames) {
    if (!m_enabled) {
        return false;
    }
    
    std::wstring normalizedKey = NormalizeKeyPath(keyPath);
    auto keyIt = m_keys.find(normalizedKey);
    
    if (keyIt == m_keys.end()) {
        return false;
    }
    
    valueNames.clear();
    for (const auto& valuePair : keyIt->second.values) {
        valueNames.push_back(valuePair.second.valueName);
    }
    
    return true;
}
