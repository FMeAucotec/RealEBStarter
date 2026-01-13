<#
.SYNOPSIS
    Extrahiert alle CLSIDs aus COM DLLs (native und managed) - FIXED VERSION
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Path,
    
    [Parameter(Mandatory=$false)]
    [string]$OutputFile = "COM_CLSIDs.ini",
    
    [Parameter(Mandatory=$false)]
    [switch]$Recursive
)

# KORRIGIERTER C# Code für PE Header Analyse
$PEAnalyzer = @"
using System;
using System.IO;
using System.Runtime.InteropServices;

public class PEAnalyzer
{
    private const ushort IMAGE_DOS_SIGNATURE = 0x5A4D;     // MZ
    private const uint IMAGE_NT_SIGNATURE = 0x00004550;    // PE00
    private const ushort IMAGE_FILE_MACHINE_I386 = 0x014C;
    private const ushort IMAGE_FILE_MACHINE_AMD64 = 0x8664;
    private const ushort IMAGE_NT_OPTIONAL_HDR32_MAGIC = 0x10B;
    private const ushort IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20B;

    public static bool IsDotNetAssembly(string filePath)
    {
        try
        {
            using (FileStream fs = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
            using (BinaryReader reader = new BinaryReader(fs))
            {
                // DOS Header lesen
                if (fs.Length < 64)
                    return false;

                ushort dosMagic = reader.ReadUInt16();
                if (dosMagic != IMAGE_DOS_SIGNATURE)
                    return false;

                // Zu PE Offset springen
                fs.Seek(0x3C, SeekOrigin.Begin);
                uint peOffset = reader.ReadUInt32();

                if (peOffset == 0 || peOffset >= fs.Length)
                    return false;

                // PE Signature prüfen
                fs.Seek(peOffset, SeekOrigin.Begin);
                uint peSignature = reader.ReadUInt32();
                
                if (peSignature != IMAGE_NT_SIGNATURE)
                    return false;

                // File Header lesen (20 bytes)
                ushort machine = reader.ReadUInt16();
                ushort numberOfSections = reader.ReadUInt16();
                uint timeDateStamp = reader.ReadUInt32();
                uint pointerToSymbolTable = reader.ReadUInt32();
                uint numberOfSymbols = reader.ReadUInt32();
                ushort sizeOfOptionalHeader = reader.ReadUInt16();
                ushort characteristics = reader.ReadUInt16();

                if (sizeOfOptionalHeader == 0)
                    return false;

                // Optional Header Magic
                ushort magic = reader.ReadUInt16();
                
                bool isPE32Plus = (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
                bool isPE32 = (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC);

                if (!isPE32 && !isPE32Plus)
                    return false;

                // Zu Data Directories springen
                // PE32:  Standard fields (28) + Windows fields (68) = 96 bytes vor DataDirectory
                // PE32+: Standard fields (24) + Windows fields (88) = 112 bytes vor DataDirectory
                int optionalHeaderStart = (int)fs.Position - 2; // -2 weil wir Magic schon gelesen haben
                
                int dataDirectoryOffset;
                if (isPE32Plus)
                {
                    // PE32+: 24 (standard) + 88 (windows) - 2 (magic bereits gelesen) = 110
                    dataDirectoryOffset = optionalHeaderStart + 110;
                }
                else
                {
                    // PE32: 28 (standard) + 68 (windows) - 2 (magic bereits gelesen) = 94
                    dataDirectoryOffset = optionalHeaderStart + 94;
                }

                fs.Seek(dataDirectoryOffset, SeekOrigin.Begin);

                // Data Directories durchgehen
                // Index 14 (0-based) ist COM Descriptor (.NET CLR Runtime Header)
                for (int i = 0; i < 15; i++)
                {
                    uint virtualAddress = reader.ReadUInt32();
                    uint size = reader.ReadUInt32();
                    
                    if (i == 14) // COM Descriptor
                    {
                        return (virtualAddress != 0 && size != 0);
                    }
                }

                return false;
            }
        }
        catch (Exception ex)
        {
            // Debug output
            Console.WriteLine("Error analyzing " + filePath + ": " + ex.Message);
            return false;
        }
    }

    public static string GetArchitecture(string filePath)
    {
        try
        {
            using (FileStream fs = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
            using (BinaryReader reader = new BinaryReader(fs))
            {
                if (fs.Length < 64)
                    return "Unknown";

                ushort dosMagic = reader.ReadUInt16();
                if (dosMagic != IMAGE_DOS_SIGNATURE)
                    return "Unknown";

                fs.Seek(0x3C, SeekOrigin.Begin);
                uint peOffset = reader.ReadUInt32();

                if (peOffset == 0 || peOffset >= fs.Length)
                    return "Unknown";

                fs.Seek(peOffset, SeekOrigin.Begin);
                uint peSignature = reader.ReadUInt32();
                
                if (peSignature != IMAGE_NT_SIGNATURE)
                    return "Unknown";

                // Machine type lesen
                ushort machine = reader.ReadUInt16();

                switch (machine)
                {
                    case IMAGE_FILE_MACHINE_I386:
                        return "x86";
                    case IMAGE_FILE_MACHINE_AMD64:
                        return "x64";
                    case 0x01C4: // ARM
                        return "ARM";
                    case 0xAA64: // ARM64
                        return "ARM64";
                    default:
                        return string.Format("Unknown (0x{0:X4})", machine);
                }
            }
        }
        catch
        {
            return "Unknown";
        }
    }
}
"@

# KORRIGIERTER Native COM Analyzer
$NativeCOMAnalyzer = @"
using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

public class NativeCOMAnalyzer
{
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr LoadLibraryEx(
        string lpFileName, 
        IntPtr hFile, 
        uint dwFlags);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr hModule);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi, ExactSpelling = true)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

    private const uint LOAD_LIBRARY_AS_DATAFILE = 0x00000002;
    private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001;

    public static Dictionary<string, bool> AnalyzeCOMExports(string dllPath)
    {
        Dictionary<string, bool> exports = new Dictionary<string, bool>();
        IntPtr hModule = IntPtr.Zero;

        try
        {
            // Versuche als Datafile zu laden (kein Code ausführen)
            hModule = LoadLibraryEx(
                dllPath, 
                IntPtr.Zero, 
                DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);

            if (hModule == IntPtr.Zero)
            {
                // Fallback: Normales LoadLibrary
                hModule = LoadLibraryEx(dllPath, IntPtr.Zero, 0);
            }

            if (hModule == IntPtr.Zero)
            {
                int error = Marshal.GetLastWin32Error();
                exports["ERROR"] = false;
                exports["ErrorCode"] = false; // Wert = error, aber bool für Dictionary
                return exports;
            }

            // Standard COM Exports prüfen
            string[] comFunctions = new string[] 
            {
                "DllGetClassObject",
                "DllCanUnloadNow",
                "DllRegisterServer",
                "DllUnregisterServer",
                "DllInstall"
            };

            foreach (string funcName in comFunctions)
            {
                IntPtr procAddr = GetProcAddress(hModule, funcName);
                exports[funcName] = (procAddr != IntPtr.Zero);
            }
        }
        catch (Exception ex)
        {
            exports["EXCEPTION"] = false;
            exports[ex.Message] = false;
        }
        finally
        {
            if (hModule != IntPtr.Zero)
            {
                FreeLibrary(hModule);
            }
        }

        return exports;
    }

    public static bool IsCOMDll(string dllPath)
    {
        var exports = AnalyzeCOMExports(dllPath);
        
        // Mindestens DllGetClassObject sollte vorhanden sein
        return exports.ContainsKey("DllGetClassObject") && exports["DllGetClassObject"];
    }
}
"@

# Compile C# Code
try {
    Add-Type -TypeDefinition $PEAnalyzer -Language CSharp -ErrorAction Stop
    Add-Type -TypeDefinition $NativeCOMAnalyzer -Language CSharp -ErrorAction Stop
    Write-Host "✓ C# Analyzers compiled successfully" -ForegroundColor Green
} catch {
    Write-Error "Failed to compile C# code: $_"
    exit 1
}

function Get-DotNetCLSIDs {
    param([string]$DllPath)
    
    $clsids = @()
    
    try {
        # Versuche Assembly zu laden
        $rawAssembly = [System.IO.File]::ReadAllBytes($DllPath)
        $assembly = [System.Reflection.Assembly]::Load($rawAssembly)
        
        # Alle exportierten Typen durchsuchen
        foreach ($type in $assembly.GetExportedTypes()) {
            try {
                # Prüfen ob COM visible
                $isComVisible = $false
                
                # Assembly-Level ComVisible
                $assemblyComVisible = $assembly.GetCustomAttributes(
                    [System.Runtime.InteropServices.ComVisibleAttribute], $false)
                
                if ($assemblyComVisible.Length -gt 0) {
                    $isComVisible = $assemblyComVisible[0].Value
                }
                
                # Type-Level ComVisible (überschreibt Assembly-Level)
                $typeComVisible = $type.GetCustomAttributes(
                    [System.Runtime.InteropServices.ComVisibleAttribute], $false)
                
                if ($typeComVisible.Length -gt 0) {
                    $isComVisible = $typeComVisible[0].Value
                }
                
                if (-not $isComVisible) {
                    continue
                }
                
                # GUID Attribut suchen
                $guidAttrs = $type.GetCustomAttributes(
                    [System.Runtime.InteropServices.GuidAttribute], $false)
                
                if ($guidAttrs.Length -gt 0) {
                    $guid = $guidAttrs[0].Value
                    
                    # ProgID suchen
                    $progIdAttrs = $type.GetCustomAttributes(
                        [System.Runtime.InteropServices.ProgIdAttribute], $false)
                    
                    $progId = if ($progIdAttrs.Length -gt 0) { 
                        $progIdAttrs[0].Value 
                    } else { 
                        "" 
                    }
                    
                    # ClassInterface prüfen
                    $classInterfaceAttrs = $type.GetCustomAttributes(
                        [System.Runtime.InteropServices.ClassInterfaceAttribute], $false)
                    
                    $classInterface = if ($classInterfaceAttrs.Length -gt 0) {
                        $classInterfaceAttrs[0].Value.ToString()
                    } else {
                        "None"
                    }
                    
                    $clsids += [PSCustomObject]@{
                        CLSID = "{$($guid.ToUpper())}"
                        ProgID = $progId
                        ClassName = $type.FullName
                        ClassInterface = $classInterface
                        IsPublic = $type.IsPublic
                    }
                }
            }
            catch {
                Write-Verbose "Could not analyze type $($type.FullName): $_"
            }
        }
    }
    catch {
        Write-Verbose "Could not load assembly $DllPath : $_"
    }
    
    return $clsids
}

function Get-RegistryCLSIDs {
    param([string]$DllPath)
    
    $clsids = @()
    $dllName = [System.IO.Path]::GetFileName($DllPath)
    
    # Registry durchsuchen
    $registryPaths = @(
        "Registry::HKEY_CLASSES_ROOT\CLSID",
        "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID",
        "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Classes\CLSID",
        "Registry::HKEY_CURRENT_USER\SOFTWARE\Classes\CLSID"
    )
    
    foreach ($regPath in $registryPaths) {
        if (Test-Path $regPath) {
            try {
                Get-ChildItem $regPath -ErrorAction SilentlyContinue | ForEach-Object {
                    $clsidKey = $_
                    $inprocPath = Join-Path $clsidKey.PSPath "InprocServer32"
                    
                    if (Test-Path $inprocPath) {
                        try {
                            $inprocKey = Get-Item $inprocPath
                            $server = $inprocKey.GetValue('')
                            
                            # Vergleiche Pfade (normalisiert)
                            $normalizedServer = [System.IO.Path]::GetFullPath($server) -replace '\\$',''
                            $normalizedDllPath = [System.IO.Path]::GetFullPath($DllPath) -replace '\\$',''
                            
                            if ($normalizedServer -eq $normalizedDllPath -or 
                                $server -like "*$dllName" -or
                                [System.IO.Path]::GetFileName($server) -eq $dllName) {
                                
                                $clsid = $clsidKey.PSChildName
                                $progId = ""
                                $className = ""
                                
                                # ProgID suchen
                                $progIdPath = Join-Path $clsidKey.PSPath "ProgID"
                                if (Test-Path $progIdPath) {
                                    $progId = (Get-ItemProperty $progIdPath).'(default)'
                                }
                                
                                # Versioned ProgID
                                $versionedProgIdPath = Join-Path $clsidKey.PSPath "VersionIndependentProgID"
                                if (Test-Path $versionedProgIdPath) {
                                    $vProgId = (Get-ItemProperty $versionedProgIdPath).'(default)'
                                    if ($vProgId) {
                                        $progId = $vProgId
                                    }
                                }
                                
                                # Class Name aus Default Value
                                $className = $clsidKey.GetValue('')
                                
                                $clsids += [PSCustomObject]@{
                                    CLSID = $clsid
                                    ProgID = $progId
                                    ClassName = $className
                                    RegistryPath = $regPath
                                    ServerPath = $server
                                }
                            }
                        }
                        catch {
                            Write-Verbose "Error reading registry key: $_"
                        }
                    }
                }
            }
            catch {
                Write-Verbose "Error accessing registry path $regPath : $_"
            }
        }
    }
    
    return $clsids
}

function Write-INIFile {
    param(
        [string]$FilePath,
        [array]$Data
    )
    
    $content = @()
    $content += "; COM CLSIDs extracted on $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    $content += "; Generated by Get-COMCLSIDs.ps1"
    $content += "; Total DLLs: $($Data.Count)"
    $content += ""
    
    foreach ($item in $Data) {
        $content += "[$($item.DllName)]"
        $content += "Path=$($item.DllPath)"
        $content += "Type=$($item.Type)"
        $content += "Architecture=$($item.Architecture)"
        $content += "IsDotNet=$($item.IsDotNet)"
        $content += "HasCOMExports=$($item.HasCOMExports)"
        
        # Native COM Exports Details
        if ($item.COMExports) {
            foreach ($export in $item.COMExports.Keys | Sort-Object) {
                $content += "Export_$export=$($item.COMExports[$export])"
            }
        }
        
        if ($item.CLSIDs.Count -gt 0) {
            $content += "CLSIDCount=$($item.CLSIDs.Count)"
            
            for ($i = 0; $i -lt $item.CLSIDs.Count; $i++) {
                $clsid = $item.CLSIDs[$i]
                $content += "CLSID$i=$($clsid.CLSID)"
                
                if ($clsid.ProgID) {
                    $content += "ProgID$i=$($clsid.ProgID)"
                }
                
                if ($clsid.ClassName) {
                    $content += "ClassName$i=$($clsid.ClassName)"
                }
                
                if ($clsid.ClassInterface) {
                    $content += "ClassInterface$i=$($clsid.ClassInterface)"
                }
                
                if ($clsid.RegistryPath) {
                    $content += "RegistrySource$i=$($clsid.RegistryPath)"
                }
            }
        } else {
            $content += "CLSIDCount=0"
        }
        
        $content += ""
    }
    
    $content | Out-File -FilePath $FilePath -Encoding UTF8
    Write-Verbose "INI file written: $FilePath"
}

# Hauptlogik
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  COM CLSID Extractor - Enhanced Version" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""
Write-Host "Scanning directory: " -NoNewline
Write-Host $Path -ForegroundColor Yellow
Write-Host "Output file: " -NoNewline
Write-Host $OutputFile -ForegroundColor Yellow
Write-Host "Recursive: " -NoNewline
Write-Host $Recursive -ForegroundColor Yellow
Write-Host ""

$searchOption = if ($Recursive) { 
    [System.IO.SearchOption]::AllDirectories 
} else { 
    [System.IO.SearchOption]::TopDirectoryOnly 
}

try {
    $dlls = [System.IO.Directory]::GetFiles($Path, "*.dll", $searchOption)
} catch {
    Write-Error "Cannot access directory: $_"
    exit 1
}

if ($dlls.Count -eq 0) {
    Write-Warning "No DLL files found in $Path"
    exit 0
}

Write-Host "Found $($dlls.Count) DLL file(s)" -ForegroundColor Green
Write-Host ""

$results = @()
$counter = 0

foreach ($dll in $dlls) {
    $counter++
    $dllName = [System.IO.Path]::GetFileName($dll)
    
    Write-Progress -Activity "Analyzing DLLs" `
                   -Status "Processing $dllName ($counter of $($dlls.Count))" `
                   -PercentComplete (($counter / $dlls.Count) * 100)
    
    Write-Host "[$counter/$($dlls.Count)] " -NoNewline -ForegroundColor Gray
    Write-Host $dllName -ForegroundColor White
    
    # PE Analyse
    $isDotNet = [PEAnalyzer]::IsDotNetAssembly($dll)
    $architecture = [PEAnalyzer]::GetArchitecture($dll)
    
    $result = [PSCustomObject]@{
        DllName = $dllName
        DllPath = $dll
        IsDotNet = $isDotNet
        Architecture = $architecture
        Type = ""
        HasCOMExports = $false
        COMExports = $null
        CLSIDs = @()
    }
    
    if ($isDotNet) {
        Write-Host "  ├─ Type: " -NoNewline -ForegroundColor Gray
        Write-Host ".NET Assembly" -ForegroundColor Cyan
        Write-Host "  ├─ Architecture: " -NoNewline -ForegroundColor Gray
        Write-Host $architecture -ForegroundColor Cyan
        
        $result.Type = ".NET COM"
        
        # .NET CLSIDs extrahieren
        $dotnetCLSIDs = Get-DotNetCLSIDs -DllPath $dll
        
        if ($dotnetCLSIDs.Count -gt 0) {
            $result.CLSIDs = $dotnetCLSIDs
            $result.HasCOMExports = $true
            Write-Host "  └─ COM Classes: " -NoNewline -ForegroundColor Gray
            Write-Host "$($dotnetCLSIDs.Count) found" -ForegroundColor Green
            
            foreach ($clsid in $dotnetCLSIDs) {
                Write-Host "     • $($clsid.CLSID)" -ForegroundColor DarkGray
                if ($clsid.ProgID) {
                    Write-Host "       ProgID: $($clsid.ProgID)" -ForegroundColor DarkGray
                }
                Write-Host "       Class: $($clsid.ClassName)" -ForegroundColor DarkGray
            }
        } else {
            Write-Host "  └─ COM Classes: " -NoNewline -ForegroundColor Gray
            Write-Host "None found" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  ├─ Type: " -NoNewline -ForegroundColor Gray
        Write-Host "Native DLL" -ForegroundColor Magenta
        Write-Host "  ├─ Architecture: " -NoNewline -ForegroundColor Gray
        Write-Host $architecture -ForegroundColor Magenta
        
        $result.Type = "Native"
        
        # Native COM Exports prüfen
        $comExports = [NativeCOMAnalyzer]::AnalyzeCOMExports($dll)
        $result.COMExports = $comExports
        
        $hasCOMExports = $comExports.ContainsKey("DllGetClassObject") -and $comExports["DllGetClassObject"]
        
        if ($hasCOMExports) {
            $result.Type = "Native COM"
            $result.HasCOMExports = $true
            
            Write-Host "  ├─ COM Exports:" -ForegroundColor Gray
            
            $exportedFuncs = @()
            foreach ($key in $comExports.Keys | Sort-Object) {
                if ($comExports[$key] -and $key -notlike "ERROR*" -and $key -ne "EXCEPTION") {
                    $exportedFuncs += $key
                    Write-Host "     ✓ $key" -ForegroundColor Green
                }
            }
            
            # Versuche CLSIDs aus Registry zu holen
            Write-Host "  ├─ Registry Lookup..." -ForegroundColor Gray -NoNewline
            $registryCLSIDs = Get-RegistryCLSIDs -DllPath $dll
            
            if ($registryCLSIDs.Count -gt 0) {
                $result.CLSIDs = $registryCLSIDs
                Write-Host " $($registryCLSIDs.Count) CLSID(s) found" -ForegroundColor Green
                
                foreach ($clsid in $registryCLSIDs) {
                    Write-Host "     • $($clsid.CLSID)" -ForegroundColor DarkGray
                    if ($clsid.ProgID) {
                        Write-Host "       ProgID: $($clsid.ProgID)" -ForegroundColor DarkGray
                    }
                    if ($clsid.ClassName) {
                        Write-Host "       Name: $($clsid.ClassName)" -ForegroundColor DarkGray
                    }
                }
            } else {
                Write-Host " Not registered" -ForegroundColor Yellow
            }
            
            Write-Host "  └─ Status: " -NoNewline -ForegroundColor Gray
            Write-Host "COM DLL" -ForegroundColor Green
        } else {
            Write-Host "  └─ Status: " -NoNewline -ForegroundColor Gray
            Write-Host "Not a COM DLL" -ForegroundColor DarkGray
        }
    }
    
    $results += $result
    Write-Host ""
}

Write-Progress -Activity "Analyzing DLLs" -Completed

# Ergebnisse schreiben
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Writing results to: $OutputFile" -ForegroundColor Cyan
Write-INIFile -FilePath $OutputFile -Data $results

# Zusammenfassung
$totalDLLs = $results.Count
$dotnetDLLs = ($results | Where-Object { $_.IsDotNet }).Count
$nativeDLLs = $totalDLLs - $dotnetDLLs
$comDLLs = ($results | Where-Object { $_.HasCOMExports }).Count
$totalCLSIDs = ($results | ForEach-Object { $_.CLSIDs.Count } | Measure-Object -Sum).Sum

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "                    SUMMARY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""
Write-Host "Total DLLs analyzed:        " -NoNewline
Write-Host $totalDLLs -ForegroundColor White
Write-Host ".NET Assemblies:            " -NoNewline
Write-Host $dotnetDLLs -ForegroundColor Cyan
Write-Host "Native DLLs:                " -NoNewline
Write-Host $nativeDLLs -ForegroundColor Magenta
Write-Host "DLLs with COM exports:      " -NoNewline
Write-Host $comDLLs -ForegroundColor Green
Write-Host "Total CLSIDs found:         " -NoNewline
Write-Host $totalCLSIDs -ForegroundColor Green
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""
Write-Host "✓ Results saved to: " -NoNewline -ForegroundColor Green
Write-Host $OutputFile -ForegroundColor Yellow
Write-Host ""
