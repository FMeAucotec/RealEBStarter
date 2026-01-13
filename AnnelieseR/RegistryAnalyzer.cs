using Microsoft.Win32;

namespace AnnelieseR
{
    public static class RegistryAnalyzer
    {
        public static List<RegistryCOMClass> GetCLSIDsForDLL(string dllPath)
        {
            List<RegistryCOMClass> classes = new List<RegistryCOMClass>();
            string dllName = Path.GetFileName(dllPath);
            string normalizedPath = Path.GetFullPath(dllPath).TrimEnd('\\').ToLowerInvariant();

            string[] registryPaths = new string[]
            {
                @"CLSID",
                @"SOFTWARE\Classes\CLSID",
                @"SOFTWARE\WOW6432Node\Classes\CLSID"
            };

            RegistryKey[] hives = new RegistryKey[]
            {
                Registry.ClassesRoot,
                Registry.LocalMachine,
                Registry.LocalMachine
            };

            for (int i = 0; i < registryPaths.Length; i++)
            {
                try
                {
                    using (RegistryKey clsidKey = hives[i].OpenSubKey(registryPaths[i]))
                    {
                        if (clsidKey == null) continue;

                        foreach (string clsidName in clsidKey.GetSubKeyNames())
                        {
                            try
                            {
                                using (RegistryKey classKey = clsidKey.OpenSubKey(clsidName))
                                {
                                    if (classKey == null) continue;

                                    using (RegistryKey inprocKey = classKey.OpenSubKey("InprocServer32"))
                                    {
                                        if (inprocKey == null) continue;

                                        string serverPath = inprocKey.GetValue("") as string;
                                        if (string.IsNullOrEmpty(serverPath)) continue;

                                        // Normalize and compare
                                        string normalizedServer;
                                        try
                                        {
                                            normalizedServer = Path.GetFullPath(serverPath)
                                                .TrimEnd('\\').ToLowerInvariant();
                                        }
                                        catch
                                        {
                                            normalizedServer = serverPath.ToLowerInvariant();
                                        }

                                        if (normalizedServer == normalizedPath ||
                                            normalizedServer.EndsWith("\\" + dllName.ToLowerInvariant()))
                                        {
                                            string progId = "";
                                            string className = classKey.GetValue("") as string ?? "";

                                            // Get ProgID
                                            using (RegistryKey progIdKey = classKey.OpenSubKey("ProgID"))
                                            {
                                                if (progIdKey != null)
                                                {
                                                    progId = progIdKey.GetValue("") as string ?? "";
                                                }
                                            }

                                            // Try version independent ProgID
                                            if (string.IsNullOrEmpty(progId))
                                            {
                                                using (RegistryKey vProgIdKey =
                                                    classKey.OpenSubKey("VersionIndependentProgID"))
                                                {
                                                    if (vProgIdKey != null)
                                                    {
                                                        progId = vProgIdKey.GetValue("") as string ?? "";
                                                    }
                                                }
                                            }

                                            classes.Add(new RegistryCOMClass
                                            {
                                                CLSID = clsidName,
                                                ProgID = progId,
                                                ClassName = className
                                            });
                                        }
                                    }
                                }
                            }
                            catch
                            {
                                // Skip classes we can't read
                            }
                        }
                    }
                }
                catch
                {
                    // Skip registry paths we can't access
                }
            }

            return classes;
        }
    }
}
