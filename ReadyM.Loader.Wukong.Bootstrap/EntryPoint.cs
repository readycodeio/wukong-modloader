using System.Reflection;
using Microsoft.Extensions.Logging;

namespace ReadyM.Loader.Wukong.Bootstrap;

// ReSharper disable once UnusedType.Global
public static class EntryPoint
{
    private static Assembly? _loaderAssembly;
    private static Type? _managedEntryPoint;

    // ReSharper disable once UnusedMember.Global
    public static void FirstStageBootstrap(long logFileHandlePtr)
    {
        FirstStageDI.Instance.Init(new IntPtr(logFileHandlePtr));
        FirstStageDI.Instance.AssemblyResolverSetup.Setup();
    }

    // ReSharper disable once UnusedMember.Global
    public static void SecondStageBootstrap(long bundledAssemblyArrayPtr, long glibNew0Ptr)
    {
        DI.Instance.Init(FirstStageDI.Instance, new IntPtr(bundledAssemblyArrayPtr), new IntPtr(glibNew0Ptr));
    }

    // ReSharper disable once UnusedMember.Global
    public static void Preprocess()
    {
        DI.Instance.BootstrapLogger.LogDebug("Preprocessing assemblies...");
        DI.Instance.AssemblyPreprocessor.Preprocess(DI.Instance.ModRegistry);
    }

    // ReSharper disable once UnusedMember.Global
    public static void Init()
    {
        DI.Instance.BootstrapLogger.LogDebug("Loading mods...");

        try
        {
            _loaderAssembly = Assembly.LoadFrom(Path.Combine(DI.Instance.PathSettings.BaseDir, "CSharpLoader", "ReadyM.Loader.Wukong.Managed.dll"));
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

        DI.Instance.BootstrapLogger.LogDebug("Loading mods complete.");
    }

    // ReSharper disable once UnusedMember.Global
    public static void LateInit()
    {
        DI.Instance.BootstrapLogger.LogDebug("Late loading mods...");

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

        DI.Instance.BootstrapLogger.LogDebug("Late loading mods complete.");
    }
}