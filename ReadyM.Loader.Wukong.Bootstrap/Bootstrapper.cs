using System.Reflection;

namespace ReadyM.Loader.Wukong.Bootstrap;

public class Bootstrapper
{
    public static void Setup()
    {
        Log.Debug("Bootstrapper: setting up callbacks");
        
        var currentDomain = AppDomain.CurrentDomain;
        currentDomain.AssemblyResolve += AssemblyResolve;
        currentDomain.UnhandledException += OnUnhandledException;

        TaskScheduler.UnobservedTaskException += OnUnobservedTaskException;

        foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
        {
            Log.Debug($"Already loaded: {asm.FullName}");
        }
        
        Log.Debug("Bootstrapper: callbacks set up");
    }

    private static Assembly? TryLoadDll(string path)
    {
        Log.Debug($"Trying to load from: {path}");
        if (File.Exists(path))
        {
            Log.Debug("Success");
            return Assembly.LoadFrom(path);
        }

        return null;
    }

    private static Assembly? AssemblyResolve(object sender, ResolveEventArgs args)
    {
        try
        {
            var dllName = $"{new AssemblyName(args.Name).Name}.dll";

            Assembly? result;
            if (ModLoaderSettings.LoadingModName == null)
            {
                // NOTE: This will prevent the assembly from being loaded
                Log.Warn($"AssemblyResolve: {args.Name} but no mod is loading");
                result = TryLoadDll(Path.Combine(ModLoaderSettings.ModDir, "Common", dllName)) ??
                         (ModLoaderSettings.CloneDir != null ? TryLoadDll(Path.Combine(ModLoaderSettings.CloneDir, "Common", dllName)) : null) ??
                         TryLoadDll(Path.Combine(ModLoaderSettings.LoaderDir, dllName));
            }
            else
            {
                result = TryLoadDll(Path.Combine(ModLoaderSettings.ModDir, ModLoaderSettings.LoadingModName, dllName)) ??
                         TryLoadDll(Path.Combine(ModLoaderSettings.ModDir, "Common", dllName)) ??
                         (ModLoaderSettings.CloneDir != null ? TryLoadDll(Path.Combine(ModLoaderSettings.CloneDir, ModLoaderSettings.LoadingModName, dllName)) : null) ??
                         (ModLoaderSettings.CloneDir != null ? TryLoadDll(Path.Combine(ModLoaderSettings.CloneDir, "Common", dllName)) : null) ??
                         TryLoadDll(Path.Combine(ModLoaderSettings.LoaderDir, dllName));
            }

            if (result != null)
                return result;
        }
        catch (Exception e)
        {
            Log.Error($"Load assembly {args.Name} failed:");
            Log.Error(e);
        }

        return Assembly.Load(args.Name);
    }
    
    private static void OnUnhandledException(object sender, UnhandledExceptionEventArgs e)
    {
        Log.Error("UnhandledException:");
        Log.Error((Exception)e.ExceptionObject);
    }

    private static void OnUnobservedTaskException(object sender, UnobservedTaskExceptionEventArgs e)
    {
        Log.Error("UnobservedTaskException:");
        Log.Error(e.Exception);
    }
}
