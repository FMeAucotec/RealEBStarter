# Example Usage

This guide shows how to use RealEBStarter to override registry values for an application.

## Scenario

You have an application `MyApp.exe` that reads configuration from the registry:
- `HKEY_CURRENT_USER\Software\MyApp\Server` value: `ServerURL`
- `HKEY_CURRENT_USER\Software\MyApp\Server` value: `Port`

You want to redirect it to a test server without modifying the actual registry.

## Step-by-Step Guide

### Step 1: Export Current Registry Values (Optional)

1. Open Registry Editor (Win+R, type `regedit`)
2. Navigate to `HKEY_CURRENT_USER\Software\MyApp\Server`
3. Right-click on `Server` key → Export
4. Save as `registry_config.reg`
5. Close Registry Editor

### Step 2: Edit the Configuration File

Open `registry_config.reg` in a text editor and modify the values:

```reg
Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\Software\MyApp\Server]
"ServerURL"="http://test-server.example.com"
"Port"=dword:00002000
"EnableDebugMode"=dword:00000001
```

### Step 3: Place Files

Place both files in the same directory:
```
C:\MyApp\
  ├── MyApp.exe
  ├── RealEBStarter.dll
  └── registry_config.reg
```

### Step 4: Inject the DLL

**Option A: Using withdll.exe (from Detours)**
```batch
cd C:\MyApp
withdll.exe /d:RealEBStarter.dll MyApp.exe
```

**Option B: Using a launcher executable**
Create a launcher app that uses `DetourCreateProcessWithDll`:
```cpp
#include <detours.h>
#include <windows.h>

int main() {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    const char* dll = "RealEBStarter.dll";
    const char* exe = "MyApp.exe";
    
    if (DetourCreateProcessWithDll(NULL, (LPSTR)exe, NULL, NULL, TRUE,
        CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &si, &pi, dll, NULL)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;
    }
    
    return 1;
}
```

**Option C: Batch file with environment variable**
```batch
@echo off
set __COMPAT_LAYER=ElevateCreateProcess
start "" "C:\MyApp\MyApp.exe"
```
Then use AppInit_DLLs or manual injection.

### Step 5: Verify

The application will now read registry values from `registry_config.reg` instead of the real registry.

Use Process Monitor (procmon.exe) to verify:
1. Run Process Monitor as Administrator
2. Set filter: `Process Name is MyApp.exe`
3. Set filter: `Operation begins with Reg`
4. Launch MyApp.exe with DLL injected
5. You should see registry operations being intercepted

## Real-World Example: Testing with Different License Keys

**Scenario**: Testing software with different license configurations without installing/uninstalling.

### Original Registry
```
HKEY_LOCAL_MACHINE\SOFTWARE\MyCompany\MyProduct
  - Edition = "Professional"
  - LicenseKey = "PROD-1234-5678-9ABC"
  - ExpiryDate = 2025-12-31
```

### Test Configuration 1: Trial Mode
`registry_config_trial.reg`:
```reg
Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\SOFTWARE\MyCompany\MyProduct]
"Edition"="Trial"
"LicenseKey"="TRIAL-KEY-INVALID"
"ExpiryDate"="2024-01-01"
"DaysRemaining"=dword:00000000
```

### Test Configuration 2: Enterprise Mode
`registry_config_enterprise.reg`:
```reg
Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\SOFTWARE\MyCompany\MyProduct]
"Edition"="Enterprise"
"LicenseKey"="ENTER-PRISE-LICE-NSEKEY"
"ExpiryDate"="2099-12-31"
"Features"=hex(7):41,00,64,00,76,00,61,00,6e,00,63,00,65,00,64,00,00,00,\
  53,00,75,00,70,00,70,00,6f,00,72,00,74,00,00,00,\
  41,00,50,00,49,00,00,00,00,00
```

### Quick Switch Script
`test_trial.bat`:
```batch
@echo off
copy /y registry_config_trial.reg registry_config.reg
withdll.exe /d:RealEBStarter.dll MyProduct.exe
```

`test_enterprise.bat`:
```batch
@echo off
copy /y registry_config_enterprise.reg registry_config.reg
withdll.exe /d:RealEBStarter.dll MyProduct.exe
```

## Advanced: Multi-Key Override

You can override multiple registry locations in a single .reg file:

```reg
Windows Registry Editor Version 5.00

; Override application settings
[HKEY_CURRENT_USER\Software\MyApp\Config]
"Server"="test.local"
"Port"=dword:00001234

; Override license information
[HKEY_LOCAL_MACHINE\SOFTWARE\MyApp]
"License"="TEST-LICENSE"
"Edition"="Professional"

; Override user preferences
[HKEY_CURRENT_USER\Software\MyApp\Preferences]
"Theme"="Dark"
"Language"="en-US"
"AutoUpdate"=dword:00000000

; Override plugin settings
[HKEY_CURRENT_USER\Software\MyApp\Plugins]
"EnabledPlugins"=hex(7):50,00,6c,00,75,00,67,00,69,00,6e,00,31,00,00,00,\
  50,00,6c,00,75,00,67,00,69,00,6e,00,32,00,00,00,00,00
```

## Debugging Tips

### Enable Debug Output

Modify `dllmain.cpp` to add logging:
```cpp
#include <fstream>

void DebugLog(const wchar_t* message) {
    std::wofstream log(L"C:\\RealEBStarter_Debug.log", std::ios::app);
    log << message << std::endl;
}

// In DLL_PROCESS_ATTACH:
if (RegistryConfig::GetInstance().LoadConfiguration(configPath)) {
    DebugLog(L"Configuration loaded successfully");
    InstallRegistryHooks();
} else {
    DebugLog(L"Failed to load configuration");
}
```

### Verify .reg File Syntax

Before using, import the .reg file into a test registry location:
1. Open regedit
2. Create a test key: `HKEY_CURRENT_USER\Software\Test`
3. Modify your .reg file to use this test location
4. File → Import → Select your .reg file
5. Check if values appear correctly
6. If yes, revert to original paths

### Check File Encoding

```powershell
# PowerShell: Check if file has UTF-16 LE BOM
$bytes = [System.IO.File]::ReadAllBytes("registry_config.reg")
if ($bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
    Write-Host "UTF-16 LE (correct for 'Windows Registry Editor Version 5.00')"
} else {
    Write-Host "ANSI (correct for 'REGEDIT4')"
}
```

## Common Patterns

### Pattern 1: Override Single Value
```reg
Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\Software\App]
"SettingToOverride"="NewValue"
```

### Pattern 2: Override Entire Key
```reg
Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\Software\App\Settings]
"Setting1"="Value1"
"Setting2"=dword:00000001
"Setting3"="Value3"
```

### Pattern 3: Override Binary Configuration
```reg
Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\Software\App]
"BinaryConfig"=hex:01,02,03,04,05,06,07,08,09,0a,0b,0c,0d,0e,0f,10
```

### Pattern 4: Environment Variable Override
```reg
Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\Environment]
"CUSTOM_VAR"="TestValue"
"PATH"=hex(2):43,00,3a,00,5c,00,54,00,65,00,73,00,74,00,00,00
```

## Security Note

This tool is for testing and development purposes. Do not use it to:
- Bypass software licensing (illegal and unethical)
- Circumvent security measures
- Distribute pirated software

Use it to:
- Test different configurations
- Debug registry-related issues
- Develop and test software
- Create isolated test environments
