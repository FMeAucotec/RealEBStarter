# Test-PEAnalyzer.ps1
param(
    [Parameter(Mandatory=$true)]
    [string]$DllPath
)

$testCode = @"
using System;
using System.IO;

public class PEDebugger
{
    public static void AnalyzePE(string filePath)
    {
        using (FileStream fs = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
        using (BinaryReader reader = new BinaryReader(fs))
        {
            Console.WriteLine("File: " + filePath);
            Console.WriteLine("Size: " + fs.Length + " bytes");
            Console.WriteLine("");
            
            // DOS Header
            ushort dosMagic = reader.ReadUInt16();
            Console.WriteLine("DOS Magic: 0x" + dosMagic.ToString("X4") + " (expected 0x5A4D)");
            
            fs.Seek(0x3C, SeekOrigin.Begin);
            uint peOffset = reader.ReadUInt32();
            Console.WriteLine("PE Offset: 0x" + peOffset.ToString("X8"));
            
            // PE Signature
            fs.Seek(peOffset, SeekOrigin.Begin);
            uint peSignature = reader.ReadUInt32();
            Console.WriteLine("PE Signature: 0x" + peSignature.ToString("X8") + " (expected 0x00004550)");
            
            // File Header
            ushort machine = reader.ReadUInt16();
            Console.WriteLine("Machine: 0x" + machine.ToString("X4"));
            Console.WriteLine("  → " + (machine == 0x014C ? "x86 (I386)" : 
                                       machine == 0x8664 ? "x64 (AMD64)" : "Unknown"));
            
            reader.ReadUInt16(); // sections
            reader.ReadUInt32(); // timestamp
            reader.ReadUInt32(); // symbol table
            reader.ReadUInt32(); // symbols
            ushort sizeOfOptionalHeader = reader.ReadUInt16();
            reader.ReadUInt16(); // characteristics
            
            Console.WriteLine("Optional Header Size: " + sizeOfOptionalHeader);
            
            // Optional Header
            ushort magic = reader.ReadUInt16();
            Console.WriteLine("Optional Header Magic: 0x" + magic.ToString("X4"));
            Console.WriteLine("  → " + (magic == 0x10B ? "PE32" : 
                                       magic == 0x20B ? "PE32+" : "Unknown"));
            
            // Zu COM Directory springen
            bool isPE32Plus = (magic == 0x20B);
            int dataDirectoryOffset = (int)fs.Position - 2 + (isPE32Plus ? 110 : 94);
            
            Console.WriteLine("");
            Console.WriteLine("Jumping to Data Directory at offset: 0x" + dataDirectoryOffset.ToString("X"));
            fs.Seek(dataDirectoryOffset, SeekOrigin.Begin);
            
            // 15 Data Directories
            string[] dirNames = {
                "Export", "Import", "Resource", "Exception", "Security",
                "BaseReloc", "Debug", "Architecture", "GlobalPtr", "TLS",
                "LoadConfig", "BoundImport", "IAT", "DelayImport", "COM Descriptor"
            };
            
            Console.WriteLine("");
            Console.WriteLine("Data Directories:");
            for (int i = 0; i < 15; i++)
            {
                uint rva = reader.ReadUInt32();
                uint size = reader.ReadUInt32();
                
                Console.WriteLine(string.Format("  [{0,2}] {1,-15}: RVA=0x{2:X8}, Size=0x{3:X8} {4}",
                    i, dirNames[i], rva, size, 
                    (rva != 0 && size != 0) ? "✓" : ""));
                
                if (i == 14 && rva != 0 && size != 0)
                {
                    Console.WriteLine("       *** .NET Assembly detected! ***");
                }
            }
        }
    }
}
"@

Add-Type -TypeDefinition $testCode
[PEDebugger]::AnalyzePE($DllPath)
