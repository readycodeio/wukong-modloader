using System.Runtime.InteropServices;

namespace ReadyM.Loader.Wukong.Bootstrap;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct MonoBundledAssembly
{
    public sbyte* name;
    public byte *data;
    public uint size;
}