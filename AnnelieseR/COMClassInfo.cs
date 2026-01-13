namespace AnnelieseR
{
    public class COMClassInfo
    {
        public string DllName { get; set; }
        public string DllPath { get; set; }
        public string CLSID { get; set; }
        public string ProgID { get; set; }
        public string ClassName { get; set; }
        public string Type { get; set; }
        public string Architecture { get; set; }
    }

    public class DotNetCOMClass
    {
        public string CLSID { get; set; }
        public string ProgID { get; set; }
        public string ClassName { get; set; }
    }

    public class RegistryCOMClass
    {
        public string CLSID { get; set; }
        public string ProgID { get; set; }
        public string ClassName { get; set; }
    }
}