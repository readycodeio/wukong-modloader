using CSharpModBase;
using Microsoft.Extensions.Logging;
using ReadyM.Loader.Wukong.Bootstrap;
using ReadyM.Loader.Wukong.Bootstrap.Registry;
using ReadyM.Loader.Wukong.Bootstrap.Settings;
using ReadyM.Loader.Wukong.Managed.Unreal;

namespace ReadyM.Loader.Wukong.Managed;

public class DI
{
    public static readonly DI Instance = new();
    
    public ILoggerFactory LoggerFactory { get; private set; } = null!;
    public ILogger LoaderLogger { get; private set; } = null!;
    
    public ModRegistry ModRegistry { get; private set; } = null!;
    public CurrentLoadingState CurrentLoadingState { get; private set; } = null!;
    public PathSettings PathSettings { get; private set; } = null!;
    
    public LoadingPhaseManager LoadingPhaseManager { get; private set; } = null!;
    public LoggerService LoggerService { get; private set; } = null!;
    
    public ModLoaderSettings ModLoaderSettings { get; private set; } = null!;
    public ModLoader ModLoader { get; private set; } = null!;
    
    public InputManager InputManager { get; private set; } = null!;
    public InputManagerService InputManagerService { get; private set; } = null!;
    
    public void Init(Bootstrap.DI bootstrapDI)
    {
        var loggerFactory = LoggerFactory = bootstrapDI.LoggerProvider.CreateLoggerFactory(true, true);
        var loaderLogger = LoaderLogger = loggerFactory.CreateLogger("ManagedLoader");

        bootstrapDI.LoggerProvider.RegisterConverter(new FTextConverter());
        
        var modRegistry = ModRegistry = bootstrapDI.ModRegistry;
        var currentLoadingState = CurrentLoadingState = bootstrapDI.CurrentLoadingState;
        var pathSettings = PathSettings = bootstrapDI.PathSettings;
        
        var loadingPhaseManager = LoadingPhaseManager = new LoadingPhaseManager(loaderLogger);
        var loggerService = LoggerService = new LoggerService(loaderLogger, loadingPhaseManager);

        var modLoaderSettingsFactory = new ModLoaderSettingsFactory(pathSettings, loaderLogger);
        var modLoaderSettings = ModLoaderSettings = modLoaderSettingsFactory.CreateSettings();
        var modLoader = ModLoader = new ModLoader(modRegistry, loadingPhaseManager, currentLoadingState, pathSettings, modLoaderSettings, loaderLogger);

        var inputManager = InputManager = InputManager.Instance;
        var inputManagerService = InputManagerService = new InputManagerService(inputManager, loadingPhaseManager);
    }
}