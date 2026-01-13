
using System;
using System.IO;

namespace AnnelieseR
{
    public static class PEAnalyzer
    {
        private const ushort IMAGE_DOS_SIGNATURE = 0x5A4D;
        private const uint IMAGE_NT_SIGNATURE = 0x00004550;
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
                    if (fs.Length < 64) return false;

                    // DOS Header
                    ushort dosMagic = reader.ReadUInt16();
                    if (dosMagic != IMAGE_DOS_SIGNATURE) return false;

                    // PE Offset
                    fs.Seek(0x3C, SeekOrigin.Begin);
                    uint peOffset = reader.ReadUInt32();
                    if (peOffset == 0 || peOffset >= fs.Length) return false;

                    // PE Signature
                    fs.Seek(peOffset, SeekOrigin.Begin);
                    uint peSignature = reader.ReadUInt32();
                    if (peSignature != IMAGE_NT_SIGNATURE) return false;

                    // File Header (skip to optional header)
                    reader.ReadUInt16(); // Machine
                    reader.ReadUInt16(); // NumberOfSections
                    reader.ReadUInt32(); // TimeDateStamp
                    reader.ReadUInt32(); // PointerToSymbolTable
                    reader.ReadUInt32(); // NumberOfSymbols
                    ushort sizeOfOptionalHeader = reader.ReadUInt16();
                    reader.ReadUInt16(); // Characteristics

                    if (sizeOfOptionalHeader == 0) return false;

                    // Optional Header
                    ushort magic = reader.ReadUInt16();
                    bool isPE32Plus = (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
                    bool isPE32 = (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC);

                    if (!isPE32 && !isPE32Plus) return false;

                    // Jump to Data Directory
                    int dataDirectoryOffset = (int)fs.Position - 2 + (isPE32Plus ? 110 : 94);
                    fs.Seek(dataDirectoryOffset, SeekOrigin.Begin);

                    // Read Data Directories (14 is COM Descriptor)
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
            catch
            {
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
                    if (fs.Length < 64) return "Unknown";

                    ushort dosMagic = reader.ReadUInt16();
                    if (dosMagic != IMAGE_DOS_SIGNATURE) return "Unknown";

                    fs.Seek(0x3C, SeekOrigin.Begin);
                    uint peOffset = reader.ReadUInt32();
                    if (peOffset == 0 || peOffset >= fs.Length) return "Unknown";

                    fs.Seek(peOffset, SeekOrigin.Begin);
                    uint peSignature = reader.ReadUInt32();
                    if (peSignature != IMAGE_NT_SIGNATURE) return "Unknown";

                    ushort machine = reader.ReadUInt16();

                    switch (machine)
                    {
                        case IMAGE_FILE_MACHINE_I386:
                            return "x86";
                        case IMAGE_FILE_MACHINE_AMD64:
                            return "x64";
                        case 0x01C4:
                            return "ARM";
                        case 0xAA64:
                            return "ARM64";
                        default:
                            return "Unknown";
                    }
                }
            }
            catch
            {
                return "Unknown";
            }
        }
    }
}

