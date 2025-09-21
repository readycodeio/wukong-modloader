using System.Reflection;
using Microsoft.Extensions.Logging;
using UnrealEngine.Runtime;

namespace ReadyM.Loader.Wukong.Bootstrap;

// ReSharper disable once UnusedType.Global
public static class EntryPoint
{
    private static Assembly? _loaderAssembly;
    private static Type? _managedEntryPoint;

    // ReSharper disable once UnusedMember.Global
    public static void InitLogging(long logFileHandlePtr)
    {
        DI.Instance.InitLogging(logFileHandlePtr);
        DI.Instance.BootstrapLogger.LogDebug("Logging initialized.");
    }

    // ReSharper disable once UnusedMember.Global
    public static unsafe void Preprocess(MonoBundledAssembly** bundledAssemblyArrayPtr, IntPtr glibNew0Ptr)
    {
        DI.Instance.BootstrapLogger.LogDebug("Initializing DI...");
        DI.Instance.Init(bundledAssemblyArrayPtr, glibNew0Ptr);
        
        DI.Instance.BootstrapLogger.LogDebug("Preprocessing assemblies...");
        DI.Instance.AssemblyPreprocessor.Preprocess(DI.Instance.ModRegistry);
    }
 
    // ReSharper disable once UnusedMember.Global
    public static void Init()
    {
        DI.Instance.BootstrapLogger.LogDebug("Bootstrapping...");
        
        try
        {
            DI.Instance.AssemblyResolverSetup.Setup();
        }
        catch (Exception ex)
        {
            DI.Instance.BootstrapLogger.LogError(ex, "Error while bootstrapping");
            return;
        }

        try
        {
            _loaderAssembly = Assembly.LoadFrom("CSharpLoader\\ReadyM.Loader.Wukong.Managed.dll");
        }
        catch (Exception ex)
        {
            DI.Instance.BootstrapLogger.LogError(ex, "Error while opening loader assembly");
            return;
        }

        _managedEntryPoint = _loaderAssembly.GetType("ReadyM.Loader.Wukong.Managed.EntryPoint");
        if (_managedEntryPoint == null)
        {
            DI.Instance.BootstrapLogger.LogError("Could not find entry point");
            return;
        }
        
        var initMethod = _managedEntryPoint.GetMethod("Init");
        if (initMethod == null)
        {
            DI.Instance.BootstrapLogger.LogError("Could not find Init method in entry point");
            return;
        }
        
        try
        {
            initMethod.Invoke(null, [DI.Instance]);
        }
        catch (Exception ex)
        {
            DI.Instance.BootstrapLogger.LogError(ex, "Error while invoking Init method");
        }

        DI.Instance.BootstrapLogger.LogDebug("Bootstrapping complete.");
    }
    
    // ReSharper disable once UnusedMember.Global
    public static void LateInit()
    {
        DI.Instance.BootstrapLogger.LogDebug("Late bootstrapping...");

        var lateInitMethod = _managedEntryPoint!.GetMethod("LateInit");
        if (lateInitMethod == null)
        {
            DI.Instance.BootstrapLogger.LogError("Could not find LateInit method in entry point");
            return;
        }
        
        try
        {
            lateInitMethod.Invoke(null, null);
        }
        catch (Exception ex)
        {
            DI.Instance.BootstrapLogger.LogError(ex, "Error while invoking LateInit method");
        }

        FCoreDelegates.OnExit.Bind(DeInit);
        
        DI.Instance.BootstrapLogger.LogDebug("Late bootstrapping complete.");
    }

    public static void DeInit()
    {
        if (_managedEntryPoint != null)
        {
            var deInitMethod = _managedEntryPoint.GetMethod("DeInit");
            if (deInitMethod == null)
            {
                DI.Instance.BootstrapLogger.LogError("Could not find DeInit method in entry point");
                return;
            }
        
            try
            {
                deInitMethod.Invoke(null, null);
            }
            catch (Exception ex)
            {
                DI.Instance.BootstrapLogger.LogError(ex, "Error while invoking DeInit method");
            }
        }
        
        DI.Instance.LoggerProvider.Dispose();
    }
}
