# RealEBStarter - Registry Hook DLL

A DLL injection tool using Microsoft Detours to hook Windows Registry API calls and provide a virtual registry from a .reg file.

## Features

- Hooks registry read/write operations (RegOpenKeyEx, RegQueryValueEx, RegSetValueEx, RegCreateKeyEx, RegDeleteValue)
- Supports both ANSI and Unicode variants of registry APIs
- Virtual registry configuration using standard Windows .reg files exported from regedit.exe
- Returns registry data directly from .reg file without touching real registry
- Read-only virtual registry (write operations return ERROR_ACCESS_DENIED)
- Supports all registry value types (REG_SZ, REG_DWORD, REG_BINARY, REG_MULTI_SZ, REG_EXPAND_SZ, etc.)

## Prerequisites

1. **Microsoft Detours Library**
   - Download from: https://github.com/microsoft/Detours
   - Build the Detours library for your target architecture
   - Create a `Detours` folder in the solution directory with the following structure:
     ```
     D:\git\RealEBStarter\Detours\
       ├── include\
       │   └── detours.h
       ├── lib.X86\
       │   └── detours.lib
       └── lib.X64\
           └── detours.lib
     ```

## Building

1. Install Microsoft Detours as described above
2. Open `RealEBStarter.sln` in Visual Studio
3. Select your target platform (x86 or x64)
4. Build the solution (Release or Debug)

## Configuration

Create a `registry_config.reg` file next to the DLL using standard Windows Registry Editor format.

### Creating the Configuration File

**Method 1: Export from regedit.exe**
1. Open Registry Editor (regedit.exe)
2. Navigate to the key you want to virtualize
3. Right-click the key → Export
4. Save as `registry_config.reg` next to the DLL
5. Edit the file to keep only the keys/values you want to override

**Method 2: Create manually**
```reg
Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\Software\MyApplication\Settings]
"ServerURL"="http://test.example.com"
"Port"=dword:00001234
"EnableDebug"=dword:00000001

[HKEY_LOCAL_MACHINE\SOFTWARE\MyCompany\MyProduct]
"Version"="1.0.0-test"
"InstallPath"="C:\\Program Files\\MyProduct\\Test"
```

### Supported Value Types

- **REG_SZ** (String): `"ValueName"="String Data"`
- **REG_DWORD** (32-bit): `"ValueName"=dword:00001234`
- **REG_BINARY**: `"ValueName"=hex:01,02,03,04`
- **REG_MULTI_SZ**: `"ValueName"=hex(7):...`
- **REG_EXPAND_SZ**: `"ValueName"=hex(2):...`
- **Other types**: `"ValueName"=hex(N):...` where N is the type number

### Configuration Notes

- The file must start with either:
  - `Windows Registry Editor Version 5.00` (Unicode format)
  - `REGEDIT4` (ANSI format)
- Comments start with `;`
- Multi-line hex values end with backslash `\`
- The virtual registry is **READ-ONLY**
- Keys not in the .reg file pass through to real registry

## Usage

### Method 1: DLL Injection
Use a DLL injection tool to inject `RealEBStarter.dll` into the target process:
```bash
# Example using withdll.exe from Detours
withdll.exe /d:RealEBStarter.dll TargetApp.exe
```

### Method 2: CreateProcess with Detours
```cpp
#include <detours.h>

STARTUPINFO si = { sizeof(si) };
PROCESS_INFORMATION pi;

LPCSTR dll = "RealEBStarter.dll";
DetourCreateProcessWithDllEx(
    NULL, "TargetApp.exe", NULL, NULL, TRUE,
    CREATE_DEFAULT_ERROR_MODE, NULL, NULL,
    &si, &pi, dll, NULL
);
```

### Method 3: AppInit_DLLs (Not recommended)
Add to registry (requires admin rights and reboot):
```
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Windows
AppInit_DLLs = C:\Path\To\RealEBStarter.dll
LoadAppInit_DLLs = 1
```

## How It Works

1. **DLL_PROCESS_ATTACH**: 
   - Loads `registry_config.reg` from the DLL directory
   - Parses all registry keys and values into memory
   - Installs Detours hooks on registry API functions

2. **Registry API Calls**:
   - **RegOpenKeyEx**: If key is in virtual registry, returns fake handle
   - **RegQueryValueEx**: If key/value is in virtual registry, returns data from memory
   - **RegSetValueEx**: If key is in virtual registry, returns ERROR_ACCESS_DENIED (read-only)
   - **RegCreateKeyEx**: If key is in virtual registry, returns ERROR_ACCESS_DENIED (read-only)
   - **RegDeleteValue**: If key is in virtual registry, returns ERROR_ACCESS_DENIED (read-only)
   - Keys NOT in virtual registry pass through to real Windows registry

3. **DLL_PROCESS_DETACH**:
   - Removes all hooks cleanly

## Hooked APIs

- RegOpenKeyExA/W
- RegQueryValueExA/W
- RegSetValueExA/W
- RegCreateKeyExA/W
- RegDeleteValueA/W

## Architecture

### Files

- `dllmain.cpp` - DLL entry point, initializes hooking
- `RegistryConfig.h/cpp` - .reg file parser and virtual registry storage
- `RegistryHooks.h/cpp` - Detours hook implementations
- `registry_config.reg` - Virtual registry configuration (user-editable)

### Key Components

1. **RegistryConfig**: Singleton class managing virtual registry
   - Loads and parses .reg file (both REGEDIT4 and Version 5.00 formats)
   - Stores all keys and values in memory
   - Provides fast lookup for registry queries
   - Thread-safe access
   - Supports all registry value types

2. **RegistryHooks**: Detours hook functions
   - Maintains map of HKEY handles to registry paths
   - Intercepts API calls and serves data from virtual registry
   - Returns fake handles for virtual keys
   - Blocks write operations to virtual keys
   - Handles both ANSI/Unicode conversions

## Debugging

Enable debug output by:
1. Adding logging to hook functions
2. Using DebugView or OutputDebugString
3. Attaching debugger to target process

## Limitations

- Hooks are process-specific (not system-wide)
- Configuration is loaded at DLL load time only (no dynamic reload)
- Virtual registry is read-only (no write/create/delete operations)
- Does not hook RegOpenKey (legacy API, use RegOpenKeyEx)
- Does not hook Nt* native registry APIs (NtOpenKey, NtQueryValueKey, etc.)
- Does not support registry enumeration (RegEnumKeyEx, RegEnumValue) for virtual keys yet
- Keys must match exactly (including root key prefix like HKEY_CURRENT_USER)

## Security Considerations

- DLL injection requires appropriate privileges
- Be cautious when redirecting system registry keys
- Test thoroughly in isolated environment first
- Some applications may detect/prevent hooking

## License

This is a starter template. Add your own license as needed.

## Building Detours

If you need to build Detours from source:

```bash
# Clone Detours
git clone https://github.com/microsoft/Detours.git

# Build for your architecture
cd Detours
nmake

# Copy files to project
mkdir ..\RealEBStarter\Detours
mkdir ..\RealEBStarter\Detours\include
mkdir ..\RealEBStarter\Detours\lib.X86
mkdir ..\RealEBStarter\Detours\lib.X64

copy include\detours.h ..\RealEBStarter\Detours\include\
copy lib.X86\detours.lib ..\RealEBStarter\Detours\lib.X86\
copy lib.X64\detours.lib ..\RealEBStarter\Detours\lib.X64\
```

## Troubleshooting

**Build Error: Cannot find detours.h**
- Ensure Detours is installed in the correct location
- Verify directory structure matches prerequisites section

**Hooks not working**
- Verify `registry_config.reg` is in same directory as DLL
- Check that .reg file starts with correct header (`Windows Registry Editor Version 5.00`)
- Ensure target process has loaded the DLL
- Use a tool like Process Monitor to verify registry accesses

**Target application crashes**
- Verify architecture match (x86 DLL with x86 app, x64 with x64)
- Check for conflicting hooks from other DLLs
- Review hook implementations for bugs
- Try with minimal .reg file first (one key/value)

**Values not being overridden**
- Registry paths are case-insensitive but must be exact matches
- Use full paths including root key (HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, etc.)
- Verify .reg file syntax is correct (export from regedit to validate)
- Check that application is actually accessing those keys
- Ensure the application uses registry APIs (not reading from files directly)

**.reg file not loading**
- Check file encoding (UTF-16 LE with BOM for Version 5.00, ANSI for REGEDIT4)
- Verify no syntax errors in .reg file
- Test by importing into regedit first
- Check for line continuation issues in hex values
