using System.Reflection;

namespace ReadyM.Loader.Wukong.Bootstrap;

// ReSharper disable once UnusedType.Global
public static class EntryPoint
{
    private static Assembly? _loaderAssembly;
    private static Type? _modLoaderEntryPoint;
    
    // ReSharper disable once UnusedMember.Global
    public static void Init()
    {
        Log.Debug("Bootstrapping");
        
        try
        {
            Bootstrapper.Setup();
        }
        catch (Exception ex)
        {
            Log.Error("Error while bootstrapping");
            Log.Error(ex);
            Log.Flush();
            return;
        }

        try
        {
            _loaderAssembly = Assembly.LoadFrom("CSharpLoader\\ReadyM.Loader.Wukong.Managed.dll");
        }
        catch (Exception ex)
        {
            Log.Error("Error while opening loader assembly");
            Log.Error(ex);
            Log.Flush();
            return;
        }

        _modLoaderEntryPoint = _loaderAssembly.GetType("ReadyM.Loader.Wukong.Managed.EntryPoint");
        if (_modLoaderEntryPoint == null)
        {
            Log.Error("Could not find entry point");
            Log.Flush();
            return;
        }
        
        var initMethod = _modLoaderEntryPoint.GetMethod("Init");
        if (initMethod == null)
        {
            Log.Error("Could not find Init method in entry point");
            Log.Flush();
            return;
        }
        
        try
        {
            initMethod.Invoke(null, null);
        }
        catch (Exception ex)
        {
            Log.Error("Error while invoking Init method");
            Log.Error(ex);
            Log.Flush();
        }

        Log.Debug("Bootstrapping complete");
        Log.Flush();
    }
}
