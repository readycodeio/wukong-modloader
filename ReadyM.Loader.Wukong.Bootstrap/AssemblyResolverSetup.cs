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
        var patchedPath = path.Replace(".dll", "_patched.dll");
        if (File.Exists(patchedPath))
        {
            return LoadFromFile(patchedPath, patched: true);
        }
        else if (File.Exists(path))
        {
            return LoadFromFile(path, patched: false);
        }

        return null;
    }

    private Assembly? LoadFromFile(string path, bool patched)
    {
        // NOTE: the "found" line goes out before the load so that a hard crash inside Assembly.LoadFrom is
        // still attributable to a specific file. The matching "Loaded" line is only written once the load
        // actually returned, so a log ending on "Found" means the load itself took the process down.
        if (patched)
            logger.LogDebug("Found (patched): {Path}", path);
        else
            logger.LogDebug("Found: {Path}", path);

        try
        {
            var asm = Assembly.LoadFrom(path);
            logger.LogDebug("Loaded: {AsmName}", asm.FullName);
            return asm;
        }
        catch (Exception ex)
        {
            // Returning null instead of letting this escape lets the remaining candidate paths be tried.
            logger.LogError(ex, "Assembly.LoadFrom failed for {Path}", path);
            return null;
        }
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
                var otherMods = Directory.GetDirectories(settings.ModDir)
                    .Select(Path.GetFileName)
                    .Where(name => name != currentLoadingState.LoadingModName);
                
                var loadedFromOtherMods = otherMods.Select(mod => TryLoadDll(Path.Combine(settings.ModDir, mod, dllName)))
                    .FirstOrDefault(asm => asm != null);
                
                result = TryLoadDll(Path.Combine(settings.ModDir, currentLoadingState.LoadingModName, dllName)) ??
                         TryLoadDll(Path.Combine(settings.ModDir, "Common", dllName)) ??
                         loadedFromOtherMods ??
                         (currentLoadingState.CloneDir != null ? TryLoadDll(Path.Combine(currentLoadingState.CloneDir, currentLoadingState.LoadingModName, dllName)) : null) ??
                         (currentLoadingState.CloneDir != null ? TryLoadDll(Path.Combine(currentLoadingState.CloneDir, "Common", dllName)) : null) ??
                         TryLoadDll(Path.Combine(settings.LoaderDir, dllName));
            }

            if (result != null)
                return result;

            logger.LogWarning(
                "Could not resolve assembly {Name} (requested by {RequestingAsmName})",
                args.Name,
                args.RequestingAssembly?.FullName ?? "<unknown>"
            );
        }
        catch (Exception ex)
        {
            logger.LogError(ex, "Load assembly {Name} failed:", args.Name);
        }

        // NOTE: must NOT fall back to Assembly.Load(args.Name) here. Mono re-enters this hook for the same
        // name, so an assembly that genuinely cannot be found recurses until the stack is gone, and a stack
        // overflow kills the process with no managed stack trace and no further log output. Returning null
        // lets the runtime finish resolution and raise a normal FileNotFoundException at the call site,
        // which the caller can catch and log.
        return null;
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
