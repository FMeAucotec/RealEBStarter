using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace AnnelieseR
{
    public static class DotNetAnalyzer
    {
        public static List<DotNetCOMClass> GetCOMClasses(string dllPath)
        {
            List<DotNetCOMClass> classes = new List<DotNetCOMClass>();

            try
            {
                byte[] rawAssembly = File.ReadAllBytes(dllPath);
                Assembly assembly = Assembly.Load(rawAssembly);

                foreach (Type type in assembly.GetExportedTypes())
                {
                    try
                    {
                        bool isComVisible = false;

                        // Check assembly-level ComVisible
                        object[] assemblyAttrs = assembly.GetCustomAttributes(
                            typeof(ComVisibleAttribute), false);
                        if (assemblyAttrs.Length > 0)
                        {
                            isComVisible = ((ComVisibleAttribute)assemblyAttrs[0]).Value;
                        }

                        // Check type-level ComVisible
                        object[] typeAttrs = type.GetCustomAttributes(
                            typeof(ComVisibleAttribute), false);
                        if (typeAttrs.Length > 0)
                        {
                            isComVisible = ((ComVisibleAttribute)typeAttrs[0]).Value;
                        }

                        if (!isComVisible) continue;

                        // Get GUID
                        object[] guidAttrs = type.GetCustomAttributes(
                            typeof(GuidAttribute), false);

                        if (guidAttrs.Length > 0)
                        {
                            string guid = ((GuidAttribute)guidAttrs[0]).Value;

                            // Get ProgID
                            string progId = "";
                            object[] progIdAttrs = type.GetCustomAttributes(
                                typeof(ProgIdAttribute), false);
                            if (progIdAttrs.Length > 0)
                            {
                                progId = ((ProgIdAttribute)progIdAttrs[0]).Value;
                            }

                            classes.Add(new DotNetCOMClass
                            {
                                CLSID = "{" + guid.ToUpper() + "}",
                                ProgID = progId,
                                ClassName = type.FullName
                            });
                        }
                    }
                    catch
                    {
                        // Skip types that can't be analyzed
                    }
                }
            }
            catch
            {
                // Could not load assembly
            }

            return classes;
        }
    }
}
