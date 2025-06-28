using ReadyM.Loader.Wukong.Bootstrap;
using UnrealEngine.Runtime;

namespace ReadyM.Loader.Wukong.Managed;

// ReSharper disable once UnusedType.Global
public static class EntryPoint
{
    // ReSharper disable once UnusedMember.Global
    public static void Init()
    {
        Log.Debug("Managed entry point init");

        var loader = ModLoader.Instance;
        
        loader.SetupDefault();
        loader.StartLogLoop();
        loader.StartInputLoop();
        
        loader.LoadMods();
        loader.InitMods();
        loader.StartLateInitMods();

        FCoreDelegates.OnExit.Bind(DeInit);
        
        Log.Debug("Managed entry init complete");
    }

    // ReSharper disable once UnusedMember.Global
    public static void DeInit()
    {
        Log.Debug("Managed entry point deinit");

        var loader = ModLoader.Instance;

        loader.Cancel();
        loader.DeInitMods();
        
        Log.Debug("Managed entry deinit complete");
    }
}
