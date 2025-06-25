using ReadyM.Loader.Wukong.Bootstrap;

namespace ReadyM.Loader.Wukong.Managed;

// ReSharper disable once UnusedType.Global
public static class EntryPoint
{
    // ReSharper disable once UnusedMember.Global
    public static void Init()
    {
        Log.Debug("Managed entry point");

        ModLoader loader = new();

        loader.SetupDefault();
        loader.StartLogLoop();
        loader.StartInputLoop();
        
        loader.LoadMods();
        //loader.PatchMods();
        loader.InitMods(false, null);
        
        Log.Debug("Managed entry exiting");
    }
}
