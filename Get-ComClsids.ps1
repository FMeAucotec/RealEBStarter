<#
.SYNOPSIS
    Extrahiert alle CLSIDs aus COM DLLs (native und managed)
.DESCRIPTION
    Scannt ein Verzeichnis nach DLLs und extrahiert:
    - CLSIDs aus nativen COM DLLs (via DllGetClassObject)
    - CLSIDs aus .NET COM DLLs (via Reflection)
    - ProgIDs wenn verfügbar
    Ausgabe in INI-Datei
.PARAMETER Path
    Verzeichnis zum Scannen
.PARAMETER OutputFile
    Ausgabe INI-Datei
.PARAMETER Recursive
    Rekursiv durch Unterverzeichnisse
.EXAMPLE
    .\Get-COMCLSIDs.ps1 -Path "C:\MyDLLs" -OutputFile "com_classes.ini"
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

# C# Code für native COM Analyse
$NativeCOMAnalyzer = @"
using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Collections.Generic;

public class NativeCOMAnalyzer
{
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr hModule);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

    // DllGetClassObject Delegate
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int DllGetClassObject(
        [In] ref Guid rclsid,
        [In] ref Guid riid,
        [Out] out IntPtr ppv
    );

    // DllRegisterServer Delegate
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int DllRegisterServer();

    public static List<string> GetCLSIDsFromNativeDLL(string dllPath)
    {
        List<string> clsids = new List<string>();
        IntPtr hModule = IntPtr.Zero;

        try
        {
            hModule = LoadLibrary(dllPath);
            if (hModule == IntPtr.Zero)
            {
                return clsids;
            }

            // Methode 1: DllGetClassObject prüfen
            IntPtr pDllGetClassObject = GetProcAddress(hModule, "DllGetClassObject");
            if (pDllGetClassObject != IntPtr.Zero)
            {
                clsids.Add("HAS_DllGetClassObject");
            }

            // Methode 2: DllRegisterServer vorhanden?
            IntPtr pDllRegisterServer = GetProcAddress(hModule, "DllRegisterServer");
            if (pDllRegisterServer != IntPtr.Zero)
            {
                clsids.Add("HAS_DllRegisterServer");
            }

            // Methode 3: Andere COM Exports
            string[] comExports = { 
                "DllCanUnloadNow", 
                "DllUnregisterServer",
                "DllInstall"
            };

            foreach (string export in comExports)
            {
                if (GetProcAddress(hModule, export) != IntPtr.Zero)
                {
                    clsids.Add("HAS_" + export);
                }
            }
        }
        catch (Exception ex)
        {
            clsids.Add("ERROR: " + ex.Message);
        }
        finally
        {
            if (hModule != IntPtr.Zero)
            {
                FreeLibrary(hModule);
            }
        }

        return clsids;
    }
}
"@

# C# Code für PE Header Analyse
$PEAnalyzer = @"
using System;
using System.IO;
using System.Runtime.InteropServices;

public class PEAnalyzer
{
    [StructLayout(LayoutKind.Sequential)]
    private struct IMAGE_DOS_HEADER
    {
        public UInt16 e_magic;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 29)]
        public UInt16[] e_res;
        public UInt32 e_lfanew;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct IMAGE_FILE_HEADER
    {
        public UInt16 Machine;
        public UInt16 NumberOfSections;
        public UInt32 TimeDateStamp;
        public UInt32 PointerToSymbolTable;
        public UInt32 NumberOfSymbols;
        public UInt16 SizeOfOptionalHeader;
        public UInt16 Characteristics;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct IMAGE_DATA_DIRECTORY
    {
        public UInt32 VirtualAddress;
        public UInt32 Size;
    }

    public static bool IsDotNetAssembly(string filePath)
    {
        try
        {
            using (FileStream fs = new FileStream(filePath, FileMode.Open, FileAccess.Read))
            using (BinaryReader reader = new BinaryReader(fs))
            {
                // DOS Header
                UInt16 dosMagic = reader.ReadUInt16();
                if (dosMagic != 0x5A4D) // "MZ"
                    return false;

                // Zu PE Header springen
                fs.Seek(0x3C, SeekOrigin.Begin);
                UInt32 peOffset = reader.ReadUInt32();

                fs.Seek(peOffset, SeekOrigin.Begin);
                UInt32 peSignature = reader.ReadUInt32();
                if (peSignature != 0x00004550) // "PE\0\0"
                    return false;

                // FILE Header überspringen
                fs.Seek(20, SeekOrigin.Current);

                // Optional Header Magic
                UInt16 magic = reader.ReadUInt16();
                
                // Zu Data Directories springen
                int offset = (magic == 0x20B) ? 110 : 94; // PE32+ oder PE32
                fs.Seek(peOffset + 24 + offset, SeekOrigin.Begin);

                // 15. Data Directory ist COM Descriptor
                for (int i = 0; i < 15; i++)
                {
                    reader.ReadUInt64(); // Skip first 14 directories
                }

                UInt32 comDescriptorRVA = reader.ReadUInt32();
                UInt32 comDescriptorSize = reader.ReadUInt32();

                return (comDescriptorRVA != 0 && comDescriptorSize != 0);
            }
        }
        catch
        {
            return false;
        }
    }

    public static bool Is64BitDLL(string filePath)
    {
        try
        {
            using (FileStream fs = new FileStream(filePath, FileMode.Open, FileAccess.Read))
            using (BinaryReader reader = new BinaryReader(fs))
            {
                fs.Seek(0x3C, SeekOrigin.Begin);
                UInt32 peOffset = reader.ReadUInt32();

                fs.Seek(peOffset + 4, SeekOrigin.Begin);
                UInt16 machine = reader.ReadUInt16();

                // 0x8664 = AMD64, 0x014C = I386
                return (machine == 0x8664);
            }
        }
        catch
        {
            return false;
        }
    }
}
"@

# Compile C# Code
Add-Type -TypeDefinition $NativeCOMAnalyzer -Language CSharp -ErrorAction SilentlyContinue
Add-Type -TypeDefinition $PEAnalyzer -Language CSharp -ErrorAction SilentlyContinue

function Get-DotNetCLSIDs {
    param([string]$DllPath)
    
    $clsids = @()
    
    try {
        # Assembly laden (nur Reflection, nicht ausführen)
        $assembly = [System.Reflection.Assembly]::ReflectionOnlyLoadFrom($DllPath)
        
        # Alle Typen durchsuchen
        foreach ($type in $assembly.GetTypes()) {
            # Prüfen ob COM visible
            $comVisibleAttr = $type.GetCustomAttributesData() | 
                Where-Object { $_.AttributeType.Name -eq "ComVisibleAttribute" }
            
            if ($comVisibleAttr -and $comVisibleAttr.ConstructorArguments[0].Value -eq $true) {
                # GUID Attribut suchen
                $guidAttr = $type.GetCustomAttributesData() | 
                    Where-Object { $_.AttributeType.Name -eq "GuidAttribute" }
                
                if ($guidAttr) {
                    $guid = $guidAttr.ConstructorArguments[0].Value
                    
                    # ProgID suchen
                    $progIdAttr = $type.GetCustomAttributesData() | 
                        Where-Object { $_.AttributeType.Name -eq "ProgIdAttribute" }
                    
                    $progId = if ($progIdAttr) { 
                        $progIdAttr.ConstructorArguments[0].Value 
                    } else { 
                        "" 
                    }
                    
                    $clsids += [PSCustomObject]@{
                        CLSID = $guid
                        ProgID = $progId
                        ClassName = $type.FullName
                    }
                }
            }
        }
    }
    catch {
        Write-Warning "Fehler beim Analysieren von $DllPath : $_"
    }
    
    return $clsids
}

function Get-RegistryCLSIDs {
    param([string]$DllPath)
    
    $clsids = @()
    $dllName = [System.IO.Path]::GetFileName($DllPath)
    
    # Registry durchsuchen
    $registryPaths = @(
        "HKLM:\SOFTWARE\Classes\CLSID",
        "HKLM:\SOFTWARE\WOW6432Node\Classes\CLSID",
        "HKCU:\SOFTWARE\Classes\CLSID"
    )
    
    foreach ($regPath in $registryPaths) {
        if (Test-Path $regPath) {
            Get-ChildItem $regPath -ErrorAction SilentlyContinue | ForEach-Object {
                $inprocPath = Join-Path $_.PSPath "InprocServer32"
                
                if (Test-Path $inprocPath) {
                    $server = (Get-ItemProperty $inprocPath).'(default)'
                    
                    if ($server -like "*$dllName*" -or $server -eq $DllPath) {
                        $clsid = $_.PSChildName
                        $progId = ""
                        
                        # ProgID suchen
                        $progIdPath = Join-Path $_.PSPath "ProgID"
                        if (Test-Path $progIdPath) {
                            $progId = (Get-ItemProperty $progIdPath).'(default)'
                        }
                        
                        $clsids += [PSCustomObject]@{
                            CLSID = $clsid
                            ProgID = $progId
                            RegistryPath = $regPath
                        }
                    }
                }
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
    $content += "; COM CLSIDs extracted on $(Get-Date)"
    $content += "; Generated by Get-COMCLSIDs.ps1"
    $content += ""
    
    foreach ($item in $Data) {
        $content += "[$($item.DllName)]"
        $content += "Path=$($item.DllPath)"
        $content += "Type=$($item.Type)"
        $content += "Architecture=$($item.Architecture)"
        $content += "IsDotNet=$($item.IsDotNet)"
        $content += "HasCOMExports=$($item.HasCOMExports)"
        
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
            }
        } else {
            $content += "CLSIDCount=0"
        }
        
        $content += ""
    }
    
    $content | Out-File -FilePath $FilePath -Encoding UTF8
}

# Hauptlogik
Write-Host "Scanning directory: $Path" -ForegroundColor Cyan
Write-Host "Output file: $OutputFile" -ForegroundColor Cyan
Write-Host ""

$searchOption = if ($Recursive) { 
    [System.IO.SearchOption]::AllDirectories 
} else { 
    [System.IO.SearchOption]::TopDirectoryOnly 
}

$dlls = [System.IO.Directory]::GetFiles($Path, "*.dll", $searchOption)
$results = @()
$counter = 0

foreach ($dll in $dlls) {
    $counter++
    $dllName = [System.IO.Path]::GetFileName($dll)
    
    Write-Progress -Activity "Analyzing DLLs" `
                   -Status "Processing $dllName ($counter of $($dlls.Count))" `
                   -PercentComplete (($counter / $dlls.Count) * 100)
    
    Write-Host "[$counter/$($dlls.Count)] Analyzing: $dllName" -ForegroundColor Yellow
    
    # PE Analyse
    $isDotNet = [PEAnalyzer]::IsDotNetAssembly($dll)
    $is64Bit = [PEAnalyzer]::Is64BitDLL($dll)
    
    $result = [PSCustomObject]@{
        DllName = $dllName
        DllPath = $dll
        IsDotNet = $isDotNet
        Architecture = if ($is64Bit) { "x64" } else { "x86" }
        Type = ""
        HasCOMExports = $false
        CLSIDs = @()
    }
    
    if ($isDotNet) {
        Write-Host "  → .NET Assembly detected" -ForegroundColor Green
        $result.Type = ".NET COM"
        
        # .NET CLSIDs extrahieren
        $dotnetCLSIDs = Get-DotNetCLSIDs -DllPath $dll
        
        if ($dotnetCLSIDs.Count -gt 0) {
            $result.CLSIDs = $dotnetCLSIDs
            $result.HasCOMExports = $true
            Write-Host "  → Found $($dotnetCLSIDs.Count) COM class(es)" -ForegroundColor Green
            
            foreach ($clsid in $dotnetCLSIDs) {
                Write-Host "    • CLSID: $($clsid.CLSID)" -ForegroundColor Gray
                if ($clsid.ProgID) {
                    Write-Host "      ProgID: $($clsid.ProgID)" -ForegroundColor Gray
                }
                Write-Host "      Class: $($clsid.ClassName)" -ForegroundColor Gray
            }
        }
    } else {
        Write-Host "  → Native DLL detected" -ForegroundColor Green
        $result.Type = "Native COM"
        
        # Native COM Exports prüfen
        $nativeExports = [NativeCOMAnalyzer]::GetCLSIDsFromNativeDLL($dll)
        
        if ($nativeExports.Count -gt 0) {
            $result.HasCOMExports = $true
            Write-Host "  → Has COM exports:" -ForegroundColor Green
            
            foreach ($export in $nativeExports) {
                Write-Host "    • $export" -ForegroundColor Gray
            }
            
            # Versuche CLSIDs aus Registry zu holen
            $registryCLSIDs = Get-RegistryCLSIDs -DllPath $dll
            
            if ($registryCLSIDs.Count -gt 0) {
                $result.CLSIDs = $registryCLSIDs
                Write-Host "  → Found $($registryCLSIDs.Count) CLSID(s) in registry" -ForegroundColor Green
                
                foreach ($clsid in $registryCLSIDs) {
                    Write-Host "    • CLSID: $($clsid.CLSID)" -ForegroundColor Gray
                    if ($clsid.ProgID) {
                        Write-Host "      ProgID: $($clsid.ProgID)" -ForegroundColor Gray
                    }
                }
            } else {
                Write-Host "  → No CLSIDs found in registry (DLL might need registration)" -ForegroundColor Yellow
            }
        }
    }
    
    $results += $result
    Write-Host ""
}

Write-Progress -Activity "Analyzing DLLs" -Completed

# Ergebnisse schreiben
Write-Host "Writing results to: $OutputFile" -ForegroundColor Cyan
Write-INIFile -FilePath $OutputFile -Data $results

# Zusammenfassung
$totalDLLs = $results.Count
$dotnetDLLs = ($results | Where-Object { $_.IsDotNet }).Count
$nativeDLLs = $totalDLLs - $dotnetDLLs
$comDLLs = ($results | Where-Object { $_.HasCOMExports }).Count
$totalCLSIDs = ($results | ForEach-Object { $_.CLSIDs.Count } | Measure-Object -Sum).Sum

Write-Host ""
Write-Host "==================== SUMMARY ====================" -ForegroundColor Cyan
Write-Host "Total DLLs analyzed:    $totalDLLs" -ForegroundColor White
Write-Host ".NET Assemblies:        $dotnetDLLs" -ForegroundColor White
Write-Host "Native DLLs:            $nativeDLLs" -ForegroundColor White
Write-Host "DLLs with COM exports:  $comDLLs" -ForegroundColor Green
Write-Host "Total CLSIDs found:     $totalCLSIDs" -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Results saved to: $OutputFile" -ForegroundColor Green
