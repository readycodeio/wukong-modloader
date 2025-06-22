using System.Reflection;

namespace ReadyM.Loader.Wukong.Bootstrap;

// ReSharper disable once UnusedType.Global
public static class EntryPoint
{
    // ReSharper disable once UnusedMember.Global
    public static void Init()
    {
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

        Assembly loaderAssembly;
        try
        {
            loaderAssembly = Assembly.LoadFrom("CSharpLoader\\ReadyM.Loader.Wukong.Managed.dll");
        }
        catch (Exception ex)
        {
            Log.Error("Error while opening loader assembly");
            Log.Error(ex);
            Log.Flush();
            return;
        }

        var modLoaderEntryPoint = loaderAssembly.GetType("ReadyM.Loader.Wukong.Managed.EntryPoint");
        if (modLoaderEntryPoint == null)
        {
            Log.Error("Could not find entry point");
            return;
        }
        
        var initMethod = modLoaderEntryPoint.GetMethod("Init");
        if (initMethod == null)
        {
            Log.Error("Could not find Init method in entry point");
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
        }

        Log.Debug("Bootstrap exit.");
    }
}
