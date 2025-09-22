using Microsoft.Extensions.Logging;
using Microsoft.Win32.SafeHandles;
using ReadyM.Loader.Wukong.Bootstrap.Logging;

namespace ReadyM.Loader.Wukong.Bootstrap;

// This contains all types that should resolve correctly before assembly resolution is set up correctly
public class FirstStageDI
{
    public static readonly FirstStageDI Instance = new();
    
    public IpcHelper IpcHelper { get; private set; } = null!;
    public PathSettingsFactory PathSettingsFactory { get; private set; } = null!;
    public PathSettings PathSettings { get; private set; } = null!;

    public LoggerFactoryProvider LoggerProvider { get; private set; } = null!;
    public ILoggerFactory LoggerFactory { get; private set; } = null!;
    public EarlyLogger EarlyLogger { get; private set; } = null!;
    public ILogger BootstrapLogger { get; private set; } = null!;

    public CurrentLoadingState CurrentLoadingState = null!;
    public AppDomainAssemblyResolverSetup AssemblyResolverSetup = null!;
    
    public void Init(IntPtr logFileHandlePtr)
    {
        var earlyLogger = EarlyLogger = new EarlyLogger();
        
        earlyLogger.LogDebug("First stage components initializing...");
        
        var ipcHelper = IpcHelper = new IpcHelper(earlyLogger);
        var pathSettingsFactory = PathSettingsFactory = new PathSettingsFactory(ipcHelper, earlyLogger);
        var pathSettings = PathSettings = pathSettingsFactory.CreateSettings();
        
        var logFileHandle = new SafeFileHandle(logFileHandlePtr, ownsHandle: false);

        var loggerProvider = LoggerProvider = new LoggerFactoryProvider(Guid.NewGuid(), logFileHandle, pathSettings);
        Log.Provider = loggerProvider;

        var loggerFactory = LoggerFactory = loggerProvider.CreateLoggerFactory(true, true);
        var bootstrapLogger = BootstrapLogger = loggerFactory.CreateLogger("Bootstrap");
        
        earlyLogger.Attach(bootstrapLogger);
        
        var currentLoadingState = CurrentLoadingState = new CurrentLoadingState();
        var assemblyResolverSetup = AssemblyResolverSetup = new(currentLoadingState, PathSettings, BootstrapLogger);
        
        earlyLogger.LogDebug("First stage components initialized.");
    }
}