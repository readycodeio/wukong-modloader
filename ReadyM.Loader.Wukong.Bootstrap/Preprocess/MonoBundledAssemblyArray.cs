using System.Runtime.InteropServices;

namespace ReadyM.Loader.Wukong.Bootstrap;

[StructLayout(LayoutKind.Sequential)]
public readonly unsafe struct MonoBundledAssemblyArray(MonoBundledAssembly** firstItemPtr)
{
    public readonly MonoBundledAssembly** FirstItemPtr = firstItemPtr;
}