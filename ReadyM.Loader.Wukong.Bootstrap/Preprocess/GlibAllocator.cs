using System.Runtime.InteropServices;

namespace PreludeLib.Tests.Preprocess;

public class GlibAllocator(IntPtr glibNew0Ptr)
{
    private readonly GlibNew0Delegate _glibNew0 = Marshal.GetDelegateForFunctionPointer<GlibNew0Delegate>(glibNew0Ptr);

    public IntPtr Alloc(int size)
        => _glibNew0(size);
}