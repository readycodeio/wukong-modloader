using Microsoft.Extensions.Logging;
using ReadyM.Loader.Wukong.Bootstrap;
using UnrealEngine.Runtime;
using Log = ReadyM.Loader.Wukong.Bootstrap.Log;

namespace ReadyM.Loader.Wukong.Managed;

// ReSharper disable once UnusedType.Global
public static class EntryPoint
{
    // ReSharper disable once UnusedMember.Global
    public static void Init(Bootstrap.DI bootstrapDI)
    {
        DI.Instance.Init(bootstrapDI);
        
        DI.Instance.LoaderLogger.LogDebug("Managed entry point init");
        DI.Instance.LoggerService.StartLogLoop();

        var legacyFactory = Log.Provider.CreateLoggerFactory(DI.Instance.ModLoaderSettings.UseDevelop, false);
        var legacyLogger = legacyFactory.CreateLogger("");
        LegacyLog.SetLogger(legacyLogger);
        
        DI.Instance.InputManagerService.StartInputLoop();
        
        DI.Instance.ModLoader.LoadMods();
        DI.Instance.ModLoader.InitMods();

        DI.Instance.LoaderLogger.LogDebug("Managed entry init complete");
        
        FCoreDelegates.OnExit.Bind(DeInit);
    }

    public static void LateInit()
    {
        DI.Instance.ModLoader.StartLateInitMods();
    }

    // ReSharper disable once UnusedMember.Global
    public static void DeInit()
    {
        DI.Instance.LoaderLogger.LogDebug("Managed entry point deinit");

        DI.Instance.LoadingPhaseManager.CancelLoading();
        DI.Instance.ModLoader.DeInitMods();
        
        DI.Instance.LoaderLogger.LogDebug("Managed entry deinit complete");
    }
}
