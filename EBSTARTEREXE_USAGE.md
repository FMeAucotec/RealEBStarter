# EBStarterExe - Quick Start Guide

EBStarterExe is a command-line tool for injecting RealEBStarter.dll into processes.

## Setup

1. Build the solution in Visual Studio
2. Ensure `EBStarterExe.exe` and `RealEBStarter.dll` are in the same directory
3. Create configuration files (`registry_config.reg`, `com_config.ini`) in the same directory

## Quick Examples

### Launch an application with hooks
```batch
EBStarterExe.exe -launch notepad.exe
```

### Launch with command-line arguments
```batch
EBStarterExe.exe -launch myapp.exe /debug /verbose
```

### Inject into running process by name
```batch
EBStarterExe.exe -inject -name notepad.exe
```

### Inject into running process by PID
```batch
EBStarterExe.exe -inject -pid 5432
```

### List all running processes
```batch
EBStarterExe.exe -list
```

### Use custom DLL path
```batch
EBStarterExe.exe -launch app.exe -dll "C:\Custom\MyHooks.dll"
```

## Command Reference

### Launch Mode
```
EBStarterExe.exe -launch <executable> [arguments]
```
- Creates a new process with the DLL injected
- Process starts suspended, DLL is injected, then process resumes
- Any arguments after the executable are passed to the application

**Examples:**
```batch
EBStarterExe.exe -launch C:\Windows\System32\calc.exe
EBStarterExe.exe -launch "C:\Program Files\MyApp\app.exe" --config test.ini
```

### Inject Mode
```
EBStarterExe.exe -inject -name <process_name>
EBStarterExe.exe -inject -pid <process_id>
```
- Injects DLL into an already running process
- Requires appropriate permissions (may need administrator)

**By Name:**
```batch
EBStarterExe.exe -inject -name chrome.exe
EBStarterExe.exe -inject -name MyApplication.exe
```

**By PID:**
```batch
EBStarterExe.exe -inject -pid 1234
```

### List Mode
```
EBStarterExe.exe -list
```
- Displays all running processes with their PIDs
- Useful for finding the target process

### Custom DLL Path
```
-dll <path>
```
- Specifies a custom path to the DLL to inject
- Can be absolute or relative
- Default: `RealEBStarter.dll` in same directory as EBStarterExe.exe

**Example:**
```batch
EBStarterExe.exe -launch app.exe -dll "D:\MyHooks\CustomHook.dll"
```

## Common Scenarios

### Test Application with Registry Overrides
1. Create `registry_config.reg` with your test values
2. Place it next to `RealEBStarter.dll`
3. Launch the application:
```batch
EBStarterExe.exe -launch "C:\MyApp\MyApp.exe"
```

### Debug Running Application
1. Start your application normally
2. Find its process name or PID:
```batch
EBStarterExe.exe -list
```
3. Inject the DLL:
```batch
EBStarterExe.exe -inject -name MyApp.exe
```

### Automated Testing
Create a batch file:
```batch
@echo off
echo Starting test environment...
copy test_config.reg registry_config.reg
EBStarterExe.exe -launch MyApp.exe /testmode
echo Test completed
```

### Multiple Instances with Different Configs
```batch
REM Terminal 1
cd C:\Tests\Config1
EBStarterExe.exe -launch app.exe

REM Terminal 2  
cd C:\Tests\Config2
EBStarterExe.exe -launch app.exe
```

## Troubleshooting

### "DLL not found"
- Ensure `RealEBStarter.dll` is in the same directory as `EBStarterExe.exe`
- Or use `-dll` to specify the full path

### "Failed to open process"
- Try running as Administrator
- Process may have anti-debugging protection
- Ensure architecture matches (x64 exe for x64 process)
- Some processes are protected (system processes, anti-cheat software)

### "Access Denied" or Error 5
- This is the most common error when trying to launch protected applications
- Many Windows system applications require Administrator privileges
- Try running EBStarterExe as Administrator
- Some applications (like UAC-protected apps) cannot be launched this way
- Consider using `-inject` on an already running process instead of `-launch`

### "Failed to create remote thread"
- Process may be protected (system process, anti-cheat)
- Some processes require special privileges
- Try launching the process directly instead of injecting

### DLL loads but hooks don't work
- Check that `registry_config.reg` and `com_config.ini` are in the same directory as the DLL
- Verify configuration file syntax
- Check that files are not locked/readonly

### Process crashes after injection
- Architecture mismatch (injecting 32-bit DLL into 64-bit process)
- DLL has errors or missing dependencies
- Target process is incompatible with hooking

## Tips

1. **Match Architecture**: Always use matching architecture
   - x64 EBStarterExe.exe → x64 RealEBStarter.dll → x64 target process
   - x86 EBStarterExe.exe → x86 RealEBStarter.dll → x86 target process

2. **Administrator Rights**: Run as admin when injecting into:
   - System processes
   - Processes running as different users
   - Protected processes

3. **Launch vs Inject**: 
   - Use `-launch` for clean injection from process start
   - Use `-inject` for debugging already running processes

4. **Configuration Location**: The DLL looks for config files in its own directory, not the target process directory

5. **Testing**: Always test with simple applications first (notepad.exe, calc.exe)

## Integration with Build Systems

### Visual Studio Post-Build
Add to your test project's post-build events:
```batch
"$(SolutionDir)x64\Debug\EBStarterExe.exe" -launch "$(TargetPath)"
```

### MSBuild
```xml
<Target Name="TestWithHooks" AfterTargets="Build">
  <Exec Command="EBStarterExe.exe -launch $(OutputPath)MyApp.exe" />
</Target>
```

### PowerShell
```powershell
$exePath = "C:\MyApp\MyApp.exe"
$ebStarter = ".\EBStarterExe.exe"

# Launch with hooks
& $ebStarter -launch $exePath
```

### Batch Script
```batch
@echo off
setlocal

set APP=MyApplication.exe
set INJECTOR=EBStarterExe.exe

REM Check if already running
tasklist /FI "IMAGENAME eq %APP%" 2>NUL | find /I /N "%APP%">NUL
if "%ERRORLEVEL%"=="0" (
    echo Application already running, injecting...
    %INJECTOR% -inject -name %APP%
) else (
    echo Launching application...
    %INJECTOR% -launch %APP%
)
```

## Exit Codes

- `0` - Success
- `1` - Error (check error message for details)

## Security Considerations

- DLL injection may be flagged by antivirus software
- Some games/applications have anti-cheat that detects injection
- Only inject into applications you own or have permission to modify
- Be aware that administrator privileges give broad system access
