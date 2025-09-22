using System.Reflection;
using Microsoft.Extensions.Logging;
using ReadyM.Loader.Wukong.Bootstrap.Settings;

namespace ReadyM.Loader.Wukong.Bootstrap;

public class AssemblyResolverSetup(CurrentLoadingState currentLoadingState, PathSettings settings, ILogger logger)
{
    public void Setup()
    {
        logger.LogDebug("Bootstrapper: setting up callbacks");
        
        var currentDomain = AppDomain.CurrentDomain;
        currentDomain.AssemblyResolve += AssemblyResolve;
        currentDomain.UnhandledException += OnUnhandledException;

        TaskScheduler.UnobservedTaskException += OnUnobservedTaskException;

        foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
        {
            logger.LogDebug("Already loaded: {AsmName}", asm.FullName);
        }
        
        logger.LogDebug("Bootstrapper: callbacks set up");
    }

    private Assembly? TryLoadDll(string path)
    {
        logger.LogDebug("Trying to load from: {Path}", path);
        if (File.Exists(path))
        {
            logger.LogDebug("Success");
            return Assembly.LoadFrom(path);
        }
        
        return null;
    }

    private Assembly? AssemblyResolve(object sender, ResolveEventArgs args)
    {
        try
        {
            var dllName = $"{new AssemblyName(args.Name).Name}.dll";

            Assembly? result;
            if (currentLoadingState.LoadingModName == null)
            {
                // NOTE: This will prevent the assembly from being loaded
                logger.LogWarning("AssemblyResolve: {Name} but no mod is loading", args.Name);
                result = TryLoadDll(Path.Combine(settings.ModDir, "Common", dllName)) ??
                         (currentLoadingState.CloneDir != null ? TryLoadDll(Path.Combine(currentLoadingState.CloneDir, "Common", dllName)) : null) ??
                         TryLoadDll(Path.Combine(settings.LoaderDir, dllName));
            }
            else
            {
                result = TryLoadDll(Path.Combine(settings.ModDir, currentLoadingState.LoadingModName, dllName)) ??
                         TryLoadDll(Path.Combine(settings.ModDir, "Common", dllName)) ??
                         (currentLoadingState.CloneDir != null ? TryLoadDll(Path.Combine(currentLoadingState.CloneDir, currentLoadingState.LoadingModName, dllName)) : null) ??
                         (currentLoadingState.CloneDir != null ? TryLoadDll(Path.Combine(currentLoadingState.CloneDir, "Common", dllName)) : null) ??
                         TryLoadDll(Path.Combine(settings.LoaderDir, dllName));
            }

            if (result != null)
                return result;
        }
        catch (Exception ex)
        {
            logger.LogError(ex, "Load assembly {Name} failed:", args.Name);
        }

        return Assembly.Load(args.Name);
    }
    
    private void OnUnhandledException(object sender, UnhandledExceptionEventArgs e)
    {
        logger.LogError(e.ExceptionObject as Exception, "UnhandledException");
    }

    private void OnUnobservedTaskException(object sender, UnobservedTaskExceptionEventArgs e)
    {
        logger.LogError(e.Exception, "UnobservedTaskException:");
    }
}
