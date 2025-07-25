using EmbedCSharpLoader.Managed;
using Microsoft.Extensions.Logging;
using ReadyM.Loader.Wukong.Bootstrap;
using Log = ReadyM.Loader.Wukong.Bootstrap.Log;

namespace ReadyM.Loader.Wukong.Managed;

// ReSharper disable once UnusedType.Global
public static class EntryPoint
{
    private static ILoggerFactory _loggerFactory = null!;
    private static ILogger _logger = null!;
    
    private static ManagedLoader _managedLoader = null!;
    
    // ReSharper disable once UnusedMember.Global
    public static void Init()
    {
        _loggerFactory = Log.Provider.CreateLoggerFactory(true, true);
        _logger = _loggerFactory.CreateLogger("ManagedLoader");
        
        _logger.LogDebug("Managed entry point init");

        var ipcHelper = new IpcHelper(_logger);
        _managedLoader = new ManagedLoader(_logger, ipcHelper);
        
        _managedLoader.SetupDefault();

        var legacyFactory = Log.Provider.CreateLoggerFactory(Settings.UseDevelop, false);
        var legacyLogger = legacyFactory.CreateLogger("");
        LegacyLog.SetLogger(legacyLogger);
        
        _managedLoader.StartLogLoop();
        _managedLoader.StartInputLoop();
        
        _managedLoader.LoadMods();
        _managedLoader.InitMods();
        _managedLoader.StartLateInitMods();

        _logger.LogDebug("Managed entry init complete");
    }

    // ReSharper disable once UnusedMember.Global
    public static void DeInit()
    {
        _logger.LogDebug("Managed entry point deinit");

        _managedLoader.Cancel();
        _managedLoader.DeInitMods();
        
        _logger.LogDebug("Managed entry deinit complete");
    }
}
