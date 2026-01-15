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
        if (pair.second.isLoaded){
            if(pair.second.hModule) 
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
    // Format 1: "CLSID=DLL_PATH"
    // Example: "{12345678-1234-1234-1234-123456789ABC}=MyComServer.dll"
    //
    // Format 2: "CLSID=shell:VERB:COMMAND:PARAMETERS"
    // Example: "{12345678-1234-1234-1234-123456789ABC}=shell:open:"notepad.exe":"C:\test.txt"
    // Example: "{12345678-1234-1234-1234-123456789ABC}=shell::"C:\Program Files\App\app.exe":"/arg1 /arg2"
    
    size_t equalPos = line.find(L'=');
    if (equalPos == std::wstring::npos) {
        return;
    }
    
    std::wstring clsidStr = Trim(line.substr(0, equalPos));
    std::wstring value = Trim(line.substr(equalPos + 1));
    
    CLSID clsid;
    if (!ParseCLSID(clsidStr, clsid)) {
        return;  // Invalid CLSID
    }
    
    COMDllMapping mapping;
    mapping.clsidString = clsidStr;
    mapping.clsid = clsid;
    mapping.hModule = nullptr;
    mapping.isLoaded = false;
    mapping.shellExecuted = false;
    
    // Check if this is a shell execute command
    if (value.find(L"shell:") == 0) {
        mapping.actionType = COMActionType::ShellExecute;
        
        // Parse shell command format: shell:VERB:COMMAND:PARAMETERS
        size_t pos = 6; // Skip "shell:"
        
        // Extract verb (can be empty or quoted)
        size_t colonPos = value.find(L':', pos);
        if (colonPos != std::wstring::npos) {
            mapping.shellVerb = Trim(value.substr(pos, colonPos - pos));
            if (mapping.shellVerb.empty()) {
                mapping.shellVerb = L"open"; // Default verb
            }
            pos = colonPos + 1;
        }
        
        // Extract command (can be quoted)
        if (pos < value.length() && value[pos] == L'"') {
            mapping.shellCommand = ExtractQuotedString(value, pos);
        } else {
            colonPos = value.find(L':', pos);
            if (colonPos != std::wstring::npos) {
                mapping.shellCommand = Trim(value.substr(pos, colonPos - pos));
                pos = colonPos + 1;
            } else {
                mapping.shellCommand = Trim(value.substr(pos));
                pos = value.length();
            }
        }
        
        // Extract parameters (can be quoted, may contain spaces)
        if (pos < value.length()) {
            if (value[pos] == L':') pos++; // Skip colon
            if (pos < value.length() && value[pos] == L'"') {
                mapping.shellParameters = ExtractQuotedString(value, pos);
            } else {
                mapping.shellParameters = Trim(value.substr(pos));
            }
        }
        
        // Resolve relative command path
        mapping.shellCommand = ResolvePath(mapping.shellCommand, m_basePath);
    } else {
        // Standard DLL path
        mapping.actionType = COMActionType::LoadDLL;
        
        // Remove quotes from DLL path if present
        std::wstring dllPath = RemoveQuotes(value);
        
        // Resolve relative path
        mapping.dllPath = ResolvePath(dllPath, m_basePath);
    }
    
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
		// pe analyze: use LoadLibraryEx with LOAD_WITH_ALTERED_SEARCH_PATH if needed

        mapping.hModule = LoadLibraryW(mapping.dllPath.c_str());
        if (!mapping.hModule) {
            mapping.managedAssembly = System::Reflection::Assembly::LoadFrom(gcnew System::String(mapping.dllPath.c_str()));
            if(nullptr == static_cast<System::Reflection::Assembly^>(mapping.managedAssembly))
                return false;
            mapping.isDotNet = true;
        }
        mapping.isLoaded = true;
    }

    if (mapping.isDotNet) {
        for each (System::Type^ type in mapping.managedAssembly->GetTypes())  {
            array<System::Object^>^ attrs = type->GetCustomAttributes(System::Runtime::InteropServices::ComVisibleAttribute::typeid, false);
            if (attrs->Length > 0 && safe_cast<System::Runtime::InteropServices::ComVisibleAttribute^>(attrs[0])->Value)  {
                System::Guid typeGuid = type->GUID;
                System::Guid clsid = System::Guid(gcnew System::String(CLSIDToString(rclsid).c_str()));
                if (typeGuid == clsid) {
                    System::Object^ obj = System::Activator::CreateInstance(type);
                    System::IntPtr pUnk = System::Runtime::InteropServices::Marshal::GetIUnknownForObject(obj);
                    System::Guid riid2  = System::Guid(gcnew System::String(CLSIDToString(riid).c_str()));
                    System::IntPtr ppv2 = System::IntPtr::Zero;
                    HRESULT hr = static_cast<HRESULT>(System::Runtime::InteropServices::Marshal::QueryInterface(pUnk, riid2, ppv2));
					ppv = reinterpret_cast<LPVOID*>(ppv2.ToPointer());
                    System::Runtime::InteropServices::Marshal::Release(pUnk);
                    return SUCCEEDED(hr);
                }
            }
		}
        return false;
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

std::wstring COMConfig::RemoveQuotes(const std::wstring& str) {
    if (str.length() >= 2 && str.front() == L'"' && str.back() == L'"') {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

std::wstring COMConfig::ExtractQuotedString(const std::wstring& str, size_t& pos) {
    // Extract a quoted string starting at position pos
    // Updates pos to point after the closing quote
    if (pos >= str.length() || str[pos] != L'"') {
        return L"";
    }
    
    pos++; // Skip opening quote
    size_t endPos = pos;
    
    // Find closing quote, handling escaped quotes
    while (endPos < str.length()) {
        if (str[endPos] == L'"') {
            // Check if it's escaped
            if (endPos + 1 < str.length() && str[endPos + 1] == L'"') {
                endPos += 2; // Skip escaped quote
                continue;
            }
            break; // Found closing quote
        }
        endPos++;
    }
    
    std::wstring result = str.substr(pos, endPos - pos);
    pos = (endPos < str.length()) ? endPos + 1 : endPos; // Move past closing quote
    
    // Replace escaped quotes
    size_t escapePos = 0;
    while ((escapePos = result.find(L"\"\"", escapePos)) != std::wstring::npos) {
        result.replace(escapePos, 2, L"\"");
        escapePos++;
    }
    
    return result;
}

bool COMConfig::IsShellExecuteAction(REFCLSID rclsid) {
    if (!m_enabled) {
        return false;
    }
    
    auto it = m_mappings.find(rclsid);
    if (it == m_mappings.end()) {
        return false;
    }
    
    return it->second.actionType == COMActionType::ShellExecute;
}

bool COMConfig::ExecuteShellCommand(REFCLSID rclsid) {
    if (!m_enabled) {
        return false;
    }
    
    auto it = m_mappings.find(rclsid);
    if (it == m_mappings.end()) {
        return false;
    }
    
    COMDllMapping& mapping = it->second;
    
    // Check if this is a shell execute action
    if (mapping.actionType != COMActionType::ShellExecute) {
        return false;
    }
    
    // Check if already executed (execute only once)
    if (mapping.shellExecuted) {
        return true; // Already executed, return success
    }
    
    // Execute the shell command
    SHELLEXECUTEINFOW sei = { 0 };
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = mapping.shellVerb.empty() ? L"open" : mapping.shellVerb.c_str();
    sei.lpFile = mapping.shellCommand.c_str();
    sei.lpParameters = mapping.shellParameters.empty() ? nullptr : mapping.shellParameters.c_str();
    sei.nShow = SW_SHOWNORMAL;
    
    BOOL result = ShellExecuteExW(&sei);
    
    if (result) {
        mapping.shellExecuted = true;
        
        // Close handle if process was created
        if (sei.hProcess) {
            CloseHandle(sei.hProcess);
        }
    }
    
    return result != FALSE;
}
