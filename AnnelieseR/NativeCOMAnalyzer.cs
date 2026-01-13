
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace AnnelieseR
{
    public static class NativeCOMAnalyzer
    {
        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);

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
                hModule = LoadLibraryEx(dllPath, IntPtr.Zero,
                    DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);

                if (hModule == IntPtr.Zero)
                {
                    hModule = LoadLibraryEx(dllPath, IntPtr.Zero, 0);
                }

                if (hModule == IntPtr.Zero)
                {
                    return exports;
                }

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
            catch
            {
                // Ignore errors
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
    }
}

