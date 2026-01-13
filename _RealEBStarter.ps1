# DLL Injection Script mit Prozessstart
# Verwendung: 
#   .\Inject-DLL.ps1 -ProcessId <PID> -DllPath <Pfad zur DLL> -ExportName <Funktionsname>
#   .\Inject-DLL.ps1 -ExecutablePath <Pfad zur EXE> -DllPath <Pfad zur DLL> -ExportName <Funktionsname> [-Arguments <Args>] [-Suspended]

param(
    [Parameter(Mandatory=$false)]
    [int]$ProcessId,
    
    [Parameter(Mandatory=$false)]
    [string]$ExecutablePath,
    
    [Parameter(Mandatory=$false)]
    [string]$Arguments,
    
    [Parameter(Mandatory=$true)]
    [string]$DllPath,
    
    [Parameter(Mandatory=$true)]
    [string]$ExportName,
    
    [Parameter(Mandatory=$false)]
    [switch]$Suspended,
    
    [Parameter(Mandatory=$false)]
    [int]$InjectionDelay = 1000
)

# P/Invoke Definitionen
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public class WinAPI {
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(
        uint dwDesiredAccess,
        bool bInheritHandle,
        int dwProcessId);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr hObject);
    
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi)]
    public static extern IntPtr LoadLibrary(string lpFileName);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool FreeLibrary(IntPtr hModule);
    
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi)]
    public static extern IntPtr GetProcAddress(
        IntPtr hModule,
        string lpProcName);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr VirtualAllocEx(
        IntPtr hProcess,
        IntPtr lpAddress,
        uint dwSize,
        uint flAllocationType,
        uint flProtect);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool VirtualFreeEx(
        IntPtr hProcess,
        IntPtr lpAddress,
        uint dwSize,
        uint dwFreeType);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool WriteProcessMemory(
        IntPtr hProcess,
        IntPtr lpBaseAddress,
        byte[] lpBuffer,
        uint nSize,
        out uint lpNumberOfBytesWritten);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr CreateRemoteThread(
        IntPtr hProcess,
        IntPtr lpThreadAttributes,
        uint dwStackSize,
        IntPtr lpStartAddress,
        IntPtr lpParameter,
        uint dwCreationFlags,
        out uint lpThreadId);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint WaitForSingleObject(
        IntPtr hHandle,
        uint dwMilliseconds);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint ResumeThread(IntPtr hThread);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint SuspendThread(IntPtr hThread);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool VirtualQuery(
        IntPtr lpAddress,
        out MEMORY_BASIC_INFORMATION lpBuffer,
        uint dwLength);
    
    [DllImport("psapi.dll", SetLastError = true)]
    public static extern bool EnumProcessModules(
        IntPtr hProcess,
        [Out] IntPtr[] lphModule,
        uint cb,
        out uint lpcbNeeded);
    
    [DllImport("psapi.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern uint GetModuleFileNameEx(
        IntPtr hProcess,
        IntPtr hModule,
        [Out] StringBuilder lpBaseName,
        uint nSize);
    
    [DllImport("psapi.dll", SetLastError = true)]
    public static extern bool GetModuleInformation(
        IntPtr hProcess,
        IntPtr hModule,
        out MODULEINFO lpmodinfo,
        uint cb);
    
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern bool CreateProcess(
        string lpApplicationName,
        string lpCommandLine,
        IntPtr lpProcessAttributes,
        IntPtr lpThreadAttributes,
        bool bInheritHandles,
        uint dwCreationFlags,
        IntPtr lpEnvironment,
        string lpCurrentDirectory,
        ref STARTUPINFO lpStartupInfo,
        out PROCESS_INFORMATION lpProcessInformation);
    
    [StructLayout(LayoutKind.Sequential)]
    public struct MEMORY_BASIC_INFORMATION {
        public IntPtr BaseAddress;
        public IntPtr AllocationBase;
        public uint AllocationProtect;
        public IntPtr RegionSize;
        public uint State;
        public uint Protect;
        public uint Type;
    }
    
    [StructLayout(LayoutKind.Sequential)]
    public struct MODULEINFO {
        public IntPtr lpBaseOfDll;
        public uint SizeOfImage;
        public IntPtr EntryPoint;
    }
    
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct STARTUPINFO {
        public int cb;
        public string lpReserved;
        public string lpDesktop;
        public string lpTitle;
        public int dwX;
        public int dwY;
        public int dwXSize;
        public int dwYSize;
        public int dwXCountChars;
        public int dwYCountChars;
        public int dwFillAttribute;
        public int dwFlags;
        public short wShowWindow;
        public short cbReserved2;
        public IntPtr lpReserved2;
        public IntPtr hStdInput;
        public IntPtr hStdOutput;
        public IntPtr hStdError;
    }
    
    [StructLayout(LayoutKind.Sequential)]
    public struct PROCESS_INFORMATION {
        public IntPtr hProcess;
        public IntPtr hThread;
        public int dwProcessId;
        public int dwThreadId;
    }
    
    // Konstanten
    public const uint PROCESS_ALL_ACCESS = 0x1F0FFF;
    public const uint MEM_COMMIT = 0x1000;
    public const uint MEM_RESERVE = 0x2000;
    public const uint MEM_RELEASE = 0x8000;
    public const uint PAGE_EXECUTE_READWRITE = 0x40;
    public const uint PAGE_READWRITE = 0x04;
    public const uint PAGE_READONLY = 0x02;
    public const uint PAGE_EXECUTE_READ = 0x20;
    
    // Process Creation Flags
    public const uint CREATE_SUSPENDED = 0x00000004;
    public const uint CREATE_NEW_CONSOLE = 0x00000010;
    public const uint NORMAL_PRIORITY_CLASS = 0x00000020;
}
"@

function Get-LastWin32Error {
    $errorCode = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    $errorMessage = [System.ComponentModel.Win32Exception]$errorCode
    return "Error $errorCode : $($errorMessage.Message)"
}

function Start-ProcessForInjection {
    param(
        [string]$ExecutablePath,
        [string]$Arguments,
        [bool]$StartSuspended
    )
    
    $si = New-Object WinAPI+STARTUPINFO
    $si.cb = [System.Runtime.InteropServices.Marshal]::SizeOf($si)
    
    $pi = New-Object WinAPI+PROCESS_INFORMATION
    
    $creationFlags = [WinAPI]::NORMAL_PRIORITY_CLASS
    if ($StartSuspended) {
        $creationFlags = $creationFlags -bor [WinAPI]::CREATE_SUSPENDED
    }
    
    $commandLine = if ($Arguments) {
        "`"$ExecutablePath`" $Arguments"
    } else {
        "`"$ExecutablePath`""
    }
    
    Write-Host "[*] Starte Prozess: $ExecutablePath" -ForegroundColor Cyan
    if ($Arguments) {
        Write-Host "[*] Mit Argumenten: $Arguments" -ForegroundColor Cyan
    }
    if ($StartSuspended) {
        Write-Host "[*] Prozess wird im suspendierten Zustand gestartet" -ForegroundColor Cyan
    }
    
    $result = [WinAPI]::CreateProcess(
        $null,
        $commandLine,
        [IntPtr]::Zero,
        [IntPtr]::Zero,
        $false,
        $creationFlags,
        [IntPtr]::Zero,
        $null,
        [ref]$si,
        [ref]$pi
    )
    
    if (-not $result) {
        throw "Konnte Prozess nicht erstellen: $(Get-LastWin32Error)"
    }
    
    Write-Host "[+] Prozess gestartet mit PID: $($pi.dwProcessId)" -ForegroundColor Green
    Write-Host "[+] Thread-ID: $($pi.dwThreadId)" -ForegroundColor Green
    
    return @{
        ProcessHandle = $pi.hProcess
        ThreadHandle = $pi.hThread
        ProcessId = $pi.dwProcessId
        ThreadId = $pi.dwThreadId
    }
}

function Get-ModuleInfo {
    param(
        [IntPtr]$ProcessHandle,
        [string]$DllPath
    )
    
    # Lade die DLL im aktuellen Prozess
    $hModule = [WinAPI]::LoadLibrary($DllPath)
    if ($hModule -eq [IntPtr]::Zero) {
        throw "Konnte DLL nicht laden: $(Get-LastWin32Error)"
    }
    
    try {
        # Hole Modul-Informationen
        $moduleInfo = New-Object WinAPI+MODULEINFO
        $currentProcess = [System.Diagnostics.Process]::GetCurrentProcess().Handle
        
        if (-not [WinAPI]::GetModuleInformation($currentProcess, $hModule, [ref]$moduleInfo, [System.Runtime.InteropServices.Marshal]::SizeOf($moduleInfo))) {
            throw "Konnte Modul-Informationen nicht abrufen: $(Get-LastWin32Error)"
        }
        
        return @{
            BaseAddress = $moduleInfo.lpBaseOfDll
            Size = $moduleInfo.SizeOfImage
            Handle = $hModule
        }
    }
    catch {
        [WinAPI]::FreeLibrary($hModule) | Out-Null
        throw
    }
}

function Get-ReadableMemorySize {
    param(
        [IntPtr]$BaseAddress
    )
    
    $mbi = New-Object WinAPI+MEMORY_BASIC_INFORMATION
    $mbiSize = [System.Runtime.InteropServices.Marshal]::SizeOf($mbi)
    
    if (-not [WinAPI]::VirtualQuery($BaseAddress, [ref]$mbi, $mbiSize)) {
        throw "Konnte Speicherinformationen nicht abfragen: $(Get-LastWin32Error)"
    }
    
    $allocationBase = $mbi.AllocationBase
    $totalSize = 0
    $currentAddress = $BaseAddress
    
    $readableProtections = 0x02 -bor 0x04 -bor 0x20 -bor 0x40 -bor 0x80 -bor 0x10
    
    while ($true) {
        if (-not [WinAPI]::VirtualQuery($currentAddress, [ref]$mbi, $mbiSize)) {
            break
        }
        
        if ($mbi.AllocationBase -ne $allocationBase) {
            break
        }
        
        if ($mbi.State -eq 0x1000 -and ($mbi.Protect -band $readableProtections)) {
            $totalSize += [int]$mbi.RegionSize
            $currentAddress = [IntPtr]::Add($currentAddress, [int]$mbi.RegionSize)
        }
        else {
            break
        }
    }
    
    return $totalSize
}

function Inject-DLL {
    param(
        [IntPtr]$ProcessHandle,
        [int]$ProcessId,
        [string]$DllPath,
        [string]$ExportName
    )
    
    try {
        Write-Host "[+] Prozess erfolgreich geöffnet" -ForegroundColor Green
        
        # Lade DLL und hole Informationen
        Write-Host "[*] Lade DLL und hole Modul-Informationen..." -ForegroundColor Cyan
        $moduleInfo = Get-ModuleInfo -ProcessHandle $ProcessHandle -DllPath $DllPath
        
        Write-Host "[+] Modul-Basis-Adresse: 0x$($moduleInfo.BaseAddress.ToString('X'))" -ForegroundColor Green
        Write-Host "[+] Modul-Größe: $($moduleInfo.Size) Bytes" -ForegroundColor Green
        
        # Hole Export-Adresse
        Write-Host "[*] Suche Export-Funktion: $ExportName" -ForegroundColor Cyan
        $procAddress = [WinAPI]::GetProcAddress($moduleInfo.Handle, $ExportName)
        
        if ($procAddress -eq [IntPtr]::Zero) {
            throw "Konnte Export-Funktion nicht finden: $(Get-LastWin32Error)"
        }
        
        Write-Host "[+] Export-Adresse gefunden: 0x$($procAddress.ToString('X'))" -ForegroundColor Green
        
        # Allokiere Speicher im Zielprozess
        Write-Host "[*] Allokiere Speicher im Zielprozess..." -ForegroundColor Cyan
        $remoteMemory = [WinAPI]::VirtualAllocEx(
            $ProcessHandle,
            $moduleInfo.BaseAddress,
            $moduleInfo.Size,
            [WinAPI]::MEM_COMMIT -bor [WinAPI]::MEM_RESERVE,
            [WinAPI]::PAGE_EXECUTE_READWRITE
        )
        
        if ($remoteMemory -eq [IntPtr]::Zero) {
            throw "Konnte Speicher nicht allokieren: $(Get-LastWin32Error)"
        }
        
        Write-Host "[+] Speicher allokiert bei: 0x$($remoteMemory.ToString('X'))" -ForegroundColor Green
        
        # Lese DLL-Daten
        Write-Host "[*] Lese DLL-Daten..." -ForegroundColor Cyan
        $readableSize = Get-ReadableMemorySize -BaseAddress $moduleInfo.BaseAddress
        $dllData = New-Object byte[] $readableSize
        [System.Runtime.InteropServices.Marshal]::Copy($moduleInfo.BaseAddress, $dllData, 0, $readableSize)
        
        Write-Host "[+] $readableSize Bytes gelesen" -ForegroundColor Green
        
        # Schreibe DLL in Zielprozess
        Write-Host "[*] Schreibe DLL in Zielprozess..." -ForegroundColor Cyan
        $bytesWritten = 0
        $result = [WinAPI]::WriteProcessMemory(
            $ProcessHandle,
            $remoteMemory,
            $dllData,
            $readableSize,
            [ref]$bytesWritten
        )
        
        if (-not $result) {
            throw "Konnte DLL nicht in Prozess schreiben: $(Get-LastWin32Error)"
        }
        
        Write-Host "[+] $bytesWritten Bytes geschrieben" -ForegroundColor Green
        
        # Erstelle Remote-Thread
        Write-Host "[*] Erstelle Remote-Thread..." -ForegroundColor Cyan
        $threadId = 0
        $hThread = [WinAPI]::CreateRemoteThread(
            $ProcessHandle,
            [IntPtr]::Zero,
            0,
            $procAddress,
            [IntPtr]::Zero,
            0,
            [ref]$threadId
        )
        
        if ($hThread -eq [IntPtr]::Zero) {
            throw "Konnte Remote-Thread nicht erstellen: $(Get-LastWin32Error)"
        }
        
        Write-Host "[+] Remote-Thread erstellt mit ID: $threadId" -ForegroundColor Green
        Write-Host "[+] DLL erfolgreich injiziert!" -ForegroundColor Green
        
        # Cleanup Thread-Handle
        [WinAPI]::CloseHandle($hThread) | Out-Null
        
        # Cleanup Module
        [WinAPI]::FreeLibrary($moduleInfo.Handle) | Out-Null
        
    }
    catch {
        throw
    }
}

# Main Script Logic
try {
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "   DLL Injection Script" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
    # Validierung der Parameter
    if (-not $ProcessId -and -not $ExecutablePath) {
        throw "Entweder -ProcessId oder -ExecutablePath muss angegeben werden!"
    }
    
    if ($ProcessId -and $ExecutablePath) {
        throw "Nur -ProcessId ODER -ExecutablePath kann angegeben werden, nicht beides!"
    }
    
    # Prüfe ob DLL existiert
    if (-not (Test-Path $DllPath)) {
        throw "DLL-Datei nicht gefunden: $DllPath"
    }
    
    $DllPath = (Resolve-Path $DllPath).Path
    Write-Host "[+] DLL-Pfad: $DllPath" -ForegroundColor Green
    Write-Host ""
    
    $hProcess = [IntPtr]::Zero
    $hThread = [IntPtr]::Zero
    $createdProcess = $false
    
    try {
        # Szenario 1: Prozess starten
        if ($ExecutablePath) {
            if (-not (Test-Path $ExecutablePath)) {
                throw "Executable nicht gefunden: $ExecutablePath"
            }
            
            $ExecutablePath = (Resolve-Path $ExecutablePath).Path
            
            $processInfo = Start-ProcessForInjection -ExecutablePath $ExecutablePath -Arguments $Arguments -StartSuspended $Suspended.IsPresent
            $ProcessId = $processInfo.ProcessId
            $hProcess = $processInfo.ProcessHandle
            $hThread = $processInfo.ThreadHandle
            $createdProcess = $true
            
            Write-Host ""
            
            # Warte kurz, damit der Prozess initialisiert wird
            if (-not $Suspended.IsPresent) {
                Write-Host "[*] Warte $InjectionDelay ms auf Prozess-Initialisierung..." -ForegroundColor Cyan
                Start-Sleep -Milliseconds $InjectionDelay
            }
        }
        # Szenario 2: Existierenden Prozess öffnen
        else {
            Write-Host "[*] Öffne existierenden Prozess mit PID: $ProcessId" -ForegroundColor Cyan
            $hProcess = [WinAPI]::OpenProcess([WinAPI]::PROCESS_ALL_ACCESS, $false, $ProcessId)
            
            if ($hProcess -eq [IntPtr]::Zero) {
                throw "Konnte Prozess nicht öffnen: $(Get-LastWin32Error)"
            }
        }
        
        # DLL injizieren
        Inject-DLL -ProcessHandle $hProcess -ProcessId $ProcessId -DllPath $DllPath -ExportName $ExportName
        
        # Wenn Prozess im suspendierten Zustand gestartet wurde, fortsetzen
        if ($createdProcess -and $Suspended.IsPresent -and $hThread -ne [IntPtr]::Zero) {
            Write-Host ""
            Write-Host "[*] Setze Hauptthread fort..." -ForegroundColor Cyan
            $resumeResult = [WinAPI]::ResumeThread($hThread)
            if ($resumeResult -eq 0xFFFFFFFF) {
                Write-Host "[!] Warnung: Konnte Thread nicht fortsetzen: $(Get-LastWin32Error)" -ForegroundColor Yellow
            } else {
                Write-Host "[+] Thread fortgesetzt (vorheriger Suspend-Count: $resumeResult)" -ForegroundColor Green
            }
        }
        
        Write-Host ""
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "   Injection erfolgreich abgeschlossen!" -ForegroundColor Green
        Write-Host "========================================" -ForegroundColor Green
        
    }
    finally {
        # Cleanup
        if ($hProcess -ne [IntPtr]::Zero) {
            [WinAPI]::CloseHandle($hProcess) | Out-Null
        }
        if ($hThread -ne [IntPtr]::Zero) {
            [WinAPI]::CloseHandle($hThread) | Out-Null
        }
    }
    
}
catch {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "   Fehler aufgetreten!" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "[!] $($_.Exception.Message)" -ForegroundColor Red
    Write-Host ""
    exit 1
}
