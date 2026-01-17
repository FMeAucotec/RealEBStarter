// EBStarterExe.cpp - DLL Injection Tool
// Injects RealEBStarter.dll into target processes

#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <sddl.h>
#include <securitybaseapi.h>
#pragma comment(lib, "advapi32.lib")

// Helper function to check if running as administrator
bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID administratorsGroup;
    
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                               DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                               &administratorsGroup)) {
        if (!CheckTokenMembership(NULL, administratorsGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(administratorsGroup);
    }
    
    return isAdmin != FALSE;
}

// Function to get directory of the current executable
std::wstring GetExeDirectory() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    std::wstring path(exePath);
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        path = path.substr(0, lastSlash + 1);
    }

    return path;
}

// Function to get full path to DLL in same directory as exe
std::wstring GetDllPath() {
    return GetExeDirectory() + L"RealEBStarter.vsl";
}

// Function to get default target executable path
std::wstring GetDefaultTargetExe() {
    std::wstring exeDir = GetExeDirectory();
    std::wstring primary = exeDir + L"EngineeringBase.exe";
    if (GetFileAttributesW(primary.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return primary;
    }

    std::wstring fallback = exeDir + L"EngineeringBaseD.exe";
    if (GetFileAttributesW(fallback.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return fallback;
    }

    return L"";
}

// Function to find process ID by name
DWORD FindProcessByName(const std::wstring& processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    
    if (!Process32FirstW(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return 0;
    }
    
    do {
        if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0) {
            CloseHandle(hSnapshot);
            return pe32.th32ProcessID;
        }
    } while (Process32NextW(hSnapshot, &pe32));
    
    CloseHandle(hSnapshot);
    return 0;
}

// Function to list all processes
void ListProcesses() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to create process snapshot" << std::endl;
        return;
    }
    
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    
    if (!Process32FirstW(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return;
    }
    
    std::wcout << L"\nRunning processes:\n";
    std::wcout << L"--------------------------------------------------\n";
    std::wcout << L"PID     Process Name\n";
    std::wcout << L"--------------------------------------------------\n";
    
    do {
        std::wcout << pe32.th32ProcessID << L"\t" << pe32.szExeFile << std::endl;
    } while (Process32NextW(hSnapshot, &pe32));
    
    CloseHandle(hSnapshot);
}

// Function to inject DLL into process
bool InjectDLL(DWORD processId, const std::wstring& dllPath) {
    // Open target process
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | 
        PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
        FALSE, processId);
    
    if (!hProcess) {
        std::wcerr << L"Failed to open process (PID: " << processId << L"). Error: " << GetLastError() << std::endl;
        std::wcerr << L"Note: You may need administrator privileges." << std::endl;
        return false;
    }
    
    // Allocate memory in target process for DLL path
    size_t dllPathSize = (dllPath.length() + 1) * sizeof(wchar_t);
    LPVOID pRemoteDllPath = VirtualAllocEx(hProcess, NULL, dllPathSize, 
                                           MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    if (!pRemoteDllPath) {
        std::wcerr << L"Failed to allocate memory in target process. Error: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return false;
    }
    
    // Write DLL path to target process memory
    SIZE_T bytesWritten;
    if (!WriteProcessMemory(hProcess, pRemoteDllPath, dllPath.c_str(), 
                           dllPathSize, &bytesWritten)) {
        std::wcerr << L"Failed to write DLL path to target process. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    
    // Get address of LoadLibraryW
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    LPVOID pLoadLibraryW = GetProcAddress(hKernel32, "LoadLibraryW");
    
    if (!pLoadLibraryW) {
        std::wcerr << L"Failed to get LoadLibraryW address. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    
    // Create remote thread to load DLL
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, 
                                       (LPTHREAD_START_ROUTINE)pLoadLibraryW,
                                       pRemoteDllPath, 0, NULL);
    
    if (!hThread) {
        std::wcerr << L"Failed to create remote thread. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    
    // Wait for thread to complete
    WaitForSingleObject(hThread, INFINITE);
    
    // Get thread exit code (DLL module handle)
    DWORD exitCode;
    GetExitCodeThread(hThread, &exitCode);
    
    // Cleanup
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemoteDllPath, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    
    if (exitCode == 0) {
        std::wcerr << L"DLL injection failed - LoadLibraryW returned NULL" << std::endl;
        return false;
    }
    
    return true;
}

// Function to launch process with DLL
bool LaunchWithDLL(const std::wstring& exePath, const std::wstring& dllPath, 
                   const std::wstring& commandLine = L"") {
    // Check if exe exists
    if (GetFileAttributesW(exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wcerr << L"Executable not found: " << exePath << std::endl;
        return false;
    }
    
    // Create suspended process
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = { 0 };
    
    std::wstring cmdLine = L"\"" + exePath + L"\"";
    if (!commandLine.empty()) {
        cmdLine += L" " + commandLine;
    }
    
    // Need non-const buffer for CreateProcessW
    std::vector<wchar_t> cmdLineBuffer(cmdLine.begin(), cmdLine.end());
    cmdLineBuffer.push_back(L'\0');
    
    if (!CreateProcessW(NULL, cmdLineBuffer.data(), NULL, NULL, FALSE,
                       CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to create process. Error: " << error << L" - ";
        
        switch (error) {
            case ERROR_FILE_NOT_FOUND:
                std::wcerr << L"File not found";
                break;
            case ERROR_PATH_NOT_FOUND:
                std::wcerr << L"Path not found";
                break;
            case ERROR_ACCESS_DENIED:
                std::wcerr << L"Access denied (try running as Administrator)";
                break;
            case ERROR_BAD_EXE_FORMAT:
                std::wcerr << L"Bad executable format (architecture mismatch)";
                break;
            case ERROR_ELEVATION_REQUIRED:
                std::wcerr << L"Elevation required (try running as Administrator)";
                break;
            case ERROR_INVALID_PARAMETER:
                std::wcerr << L"Invalid parameters";
                break;
            default:
                std::wcerr << L"Unknown error";
                break;
        }
        std::wcerr << std::endl;
        
        // Additional help for common issues
        if (error == ERROR_ACCESS_DENIED || error == ERROR_ELEVATION_REQUIRED) {
            std::wcerr << L"Note: Some applications require Administrator privileges to launch." << std::endl;
            std::wcerr << L"Try running EBStarterExe as Administrator." << std::endl;
        }
        
        return false;
    }
    
    std::wcout << L"Process created (PID: " << pi.dwProcessId << L")" << std::endl;
    
    // Inject DLL
    bool injected = InjectDLL(pi.dwProcessId, dllPath);
    
    if (injected) {
        std::wcout << L"DLL injected successfully" << std::endl;
    } else {
        std::wcerr << L"DLL injection failed" << std::endl;
    }
    
    // Resume process
    ResumeThread(pi.hThread);
    
    // Cleanup
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    
    return injected;
}

void PrintUsage() {
    std::wcout << L"RealEBStarter DLL Injection Tool\n";
    std::wcout << L"=================================\n\n";
    std::wcout << L"Usage:\n";
    std::wcout << L"  EBStarterExe.exe\n";
    std::wcout << L"  EBStarterExe.exe -launch <executable> [arguments]\n";
    std::wcout << L"  EBStarterExe.exe -inject -pid <process_id>\n";
    std::wcout << L"  EBStarterExe.exe -inject -name <process_name>\n";
    std::wcout << L"  EBStarterExe.exe -list\n\n";
    std::wcout << L"Options:\n";
    std::wcout << L"  -launch <exe>    Launch executable with DLL injected\n";
    std::wcout << L"  -inject          Inject DLL into running process\n";
    std::wcout << L"  -pid <id>        Target process by ID\n";
    std::wcout << L"  -name <name>     Target process by name (e.g., notepad.exe)\n";
    std::wcout << L"  -list            List all running processes\n";
    std::wcout << L"  -dll <path>      Specify custom DLL path (default: RealEBStarter.dll)\n\n";
    std::wcout << L"Default behavior (no args):\n";
    std::wcout << L"  Launch EngineeringBase.exe if present in the same folder\n";
    std::wcout << L"  Otherwise launch EngineeringBaseD.exe if present\n\n";
    std::wcout << L"Examples:\n";
    std::wcout << L"  EBStarterExe.exe\n";
    std::wcout << L"  EBStarterExe.exe -launch notepad.exe\n";
    std::wcout << L"  EBStarterExe.exe -launch \"C:\\Program Files\\MyApp\\app.exe\" /arg1 /arg2\n";
    std::wcout << L"  EBStarterExe.exe -inject -name notepad.exe\n";
    std::wcout << L"  EBStarterExe.exe -inject -pid 1234\n";
    std::wcout << L"  EBStarterExe.exe -list\n\n";
}

int wmain(int argc, wchar_t* argv[]) {
    // Get DLL path
    std::wstring dllPath = GetDllPath();
    
    if (argc < 2) {
        std::wstring defaultExe = GetDefaultTargetExe();
        if (defaultExe.empty()) {
            std::wcerr << L"Error: No default target found in this folder." << std::endl;
            std::wcerr << L"Expected EngineeringBase.exe or EngineeringBaseD.exe next to EBStarterExe.exe" << std::endl;
            PrintUsage();
            return 1;
        }

        if (!IsRunningAsAdmin()) {
            std::wcout << L"Warning: EBStarterExe is not running as Administrator." << std::endl;
            std::wcout << L"Some applications may require elevated privileges." << std::endl;
            std::wcout << L"If you encounter 'Access Denied' errors, try running as Administrator." << std::endl;
            std::wcout << std::endl;
        }

        std::wcout << L"Launching default target: " << defaultExe << std::endl;
        std::wcout << L"DLL: " << dllPath << std::endl << std::endl;

        if (LaunchWithDLL(defaultExe, dllPath)) {
            std::wcout << L"\nSuccess!" << std::endl;
            return 0;
        }

        return 1;
    }
    
    std::wstring command = argv[1];
    
    // Check if we're running as admin and provide warning if not
    if (command == L"-launch" || command == L"-inject") {
        if (!IsRunningAsAdmin()) {
            std::wcout << L"Warning: EBStarterExe is not running as Administrator." << std::endl;
            std::wcout << L"Some applications may require elevated privileges." << std::endl;
            std::wcout << L"If you encounter 'Access Denied' errors, try running as Administrator." << std::endl;
            std::wcout << std::endl;
        }
    }
    
    // List processes
    if (command == L"-list") {
        ListProcesses();
        return 0;
    }
    
    // Check for custom DLL path
    for (int i = 2; i < argc - 1; i++) {
        if (std::wstring(argv[i]) == L"-dll") {
            dllPath = argv[i + 1];
            break;
        }
    }
    
    // Check if DLL exists
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wcerr << L"DLL not found: " << dllPath << std::endl;
        std::wcerr << L"Place RealEBStarter.dll in the same directory as EBStarterExe.exe" << std::endl;
        return 1;
    }
    
    // Launch with DLL
    if (command == L"-launch") {
        if (argc < 3) {
            std::wcerr << L"Error: Missing executable path" << std::endl;
            PrintUsage();
            return 1;
        }
        
        std::wstring exePath = argv[2];
        std::wstring cmdLineArgs;
        
        // Collect remaining arguments
        for (int i = 3; i < argc; i++) {
            if (std::wstring(argv[i]) == L"-dll") {
                i++; // Skip -dll and its argument
                continue;
            }
            if (!cmdLineArgs.empty()) cmdLineArgs += L" ";
            cmdLineArgs += argv[i];
        }
        
        std::wcout << L"Launching: " << exePath << std::endl;
        if (!cmdLineArgs.empty()) {
            std::wcout << L"Arguments: " << cmdLineArgs << std::endl;
        }
        std::wcout << L"DLL: " << dllPath << std::endl << std::endl;
        
        if (LaunchWithDLL(exePath, dllPath, cmdLineArgs)) {
            std::wcout << L"\nSuccess!" << std::endl;
            return 0;
        } else {
            return 1;
        }
    }
    
    // Inject into running process
    if (command == L"-inject") {
        if (argc < 4) {
            std::wcerr << L"Error: Missing process identifier" << std::endl;
            PrintUsage();
            return 1;
        }
        
        std::wstring idType = argv[2];
        DWORD processId = 0;
        
        if (idType == L"-pid") {
            processId = _wtoi(argv[3]);
        } else if (idType == L"-name") {
            std::wstring processName = argv[3];
            std::wcout << L"Searching for process: " << processName << std::endl;
            processId = FindProcessByName(processName);
            
            if (processId == 0) {
                std::wcerr << L"Process not found: " << processName << std::endl;
                std::wcout << L"\nUse -list to see running processes" << std::endl;
                return 1;
            }
            std::wcout << L"Found process (PID: " << processId << L")" << std::endl;
        } else {
            std::wcerr << L"Error: Invalid option. Use -pid or -name" << std::endl;
            PrintUsage();
            return 1;
        }
        
        std::wcout << L"DLL: " << dllPath << std::endl;
        std::wcout << L"Injecting into PID: " << processId << std::endl << std::endl;
        
        if (InjectDLL(processId, dllPath)) {
            std::wcout << L"\nSuccess!" << std::endl;
            return 0;
        } else {
            return 1;
        }
    }
    
    std::wcerr << L"Error: Unknown command" << std::endl;
    PrintUsage();
    return 1;
}
