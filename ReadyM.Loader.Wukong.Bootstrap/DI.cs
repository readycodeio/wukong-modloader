using Microsoft.Extensions.Logging;
using Microsoft.Win32.SafeHandles;
using PreludeLib.CompileTime.Backend.WeaverCallback;
using PreludeLib.CompileTime.Public;
using PreludeLib.Tests.Preprocess;
using ReadyM.Loader.Wukong.Bootstrap.Logging;
using ReadyM.Loader.Wukong.Bootstrap.Preprocess;

namespace ReadyM.Loader.Wukong.Bootstrap;

public class DI
{
    public static readonly DI Instance = new();
    
    public IpcHelper IpcHelper { get; private set; } = null!;
    public PathSettingsFactory PathSettingsFactory { get; private set; } = null!;
    public PathSettings PathSettings { get; private set; } = null!;
    public LoggerFactoryProvider LoggerProvider { get; private set; } = null!;
    public ILoggerFactory LoggerFactory { get; private set; } = null!;
    public EarlyLogger EarlyLogger { get; private set; } = null!;
    public ILogger BootstrapLogger { get; private set; } = null!;

    public MonoBundledAssemblyArray BundledAssemblyArray { get; private set; } = default;
    
    public GlibAllocator Allocator { get; private set; } = null!;
    public PreprocessAssemblyResolver PreprocessAssemblyResolver { get; private set; } = null!;
    public CompileTimeWeaverBackend CompileTimeBackend { get; private set; } = null!;
    public CompileTimePrelude CompileTimePrelude { get; private set; } = null!;
    public AssemblyPreprocessor AssemblyPreprocessor { get; private set; } = null!;
    
    public ModLocator ModLocator { get; private set; } = null!;
    public ModRegistry ModRegistry { get; private set; } = null!;
    
    public CurrentLoadingState CurrentLoadingState { get; private set; } = null!;
    public AppDomainAssemblyResolverSetup AssemblyResolverSetup { get; private set; } = null!;

    public void InitLogging(long logFileHandlePtr)
    {
        var earlyLogger = EarlyLogger = new EarlyLogger();
        
        var ipcHelper = IpcHelper = new IpcHelper(earlyLogger);
        var pathSettingsFactory = PathSettingsFactory = new PathSettingsFactory(ipcHelper, earlyLogger);
        var pathSettings = PathSettings = pathSettingsFactory.CreateSettings();
        
        var logFileHandle = new SafeFileHandle(new IntPtr(logFileHandlePtr), ownsHandle: false);

        var loggerProvider = LoggerProvider = new LoggerFactoryProvider(Guid.NewGuid(), logFileHandle, pathSettings);
        Log.Provider = loggerProvider;

        var loggerFactory = LoggerFactory = loggerProvider.CreateLoggerFactory(true, true);
        var bootstrapLogger = BootstrapLogger = loggerFactory.CreateLogger("Bootstrap");
        
        earlyLogger.Attach(bootstrapLogger);
    }

    public unsafe void Init(MonoBundledAssembly** bundledAssemblyArrayPtr, IntPtr glibNew0Ptr)
    {
        var allocator = Allocator = new GlibAllocator(glibNew0Ptr);

        var bundledAssemblyArray = BundledAssemblyArray = new MonoBundledAssemblyArray(bundledAssemblyArrayPtr);
        var preprocessAssemblyResolver = PreprocessAssemblyResolver = new PreprocessAssemblyResolver(bundledAssemblyArray, allocator);
        var compileTimeBackend = CompileTimeBackend = new CompileTimeWeaverBackend(BootstrapLogger);
        var compileTimePrelude = CompileTimePrelude = new CompileTimePrelude(compileTimeBackend, BootstrapLogger);
        var assemblyPreprocessor = AssemblyPreprocessor = new AssemblyPreprocessor(preprocessAssemblyResolver, compileTimePrelude, BootstrapLogger);

        var modLocator = ModLocator = new ModLocator(PathSettings, BootstrapLogger);
        var modRegistry = ModRegistry = modLocator.LocateMods();
        
        var currentLoadingState = CurrentLoadingState = new CurrentLoadingState();
        var assemblyResolverSetup = AssemblyResolverSetup = new AppDomainAssemblyResolverSetup(currentLoadingState, PathSettings, BootstrapLogger);
        
        
    }
}