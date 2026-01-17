using System.Runtime.InteropServices;

namespace DotNetComTest;

[ComVisible(true)]
[Guid("B92636AD-2ED3-4F92-B9F9-45B64729FB4F")]
public interface IDotNetTestCom
{
    void Ping();
}

[ComVisible(true)]
[Guid("DD1FA7F8-D82B-490E-8E35-5D32182581EE")]
[ClassInterface(ClassInterfaceType.None)]
public sealed class DotNetTestCom : IDotNetTestCom
{
    public void Ping()
    {
        // No-op test call
    }
}

