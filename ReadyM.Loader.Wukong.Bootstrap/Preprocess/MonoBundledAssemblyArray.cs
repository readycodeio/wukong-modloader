using System.Runtime.InteropServices;

namespace ReadyM.Loader.Wukong.Bootstrap.Preprocess;

[StructLayout(LayoutKind.Sequential)]
public readonly unsafe struct MonoBundledAssemblyArray(MonoBundledAssembly*** firstItemPtr)
{
    public readonly MonoBundledAssembly*** ArrayPtr = firstItemPtr;
}