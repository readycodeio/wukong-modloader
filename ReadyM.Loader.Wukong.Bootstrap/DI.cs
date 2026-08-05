using Microsoft.Extensions.Logging;
using PreludeLib.CompileTime.Backend.WeaverCallback;
using PreludeLib.CompileTime.Public;
using ReadyM.Loader.Wukong.Bootstrap.Logging;
using ReadyM.Loader.Wukong.Bootstrap.Preprocess;
using ReadyM.Loader.Wukong.Bootstrap.Registry;
using ReadyM.Loader.Wukong.Bootstrap.Settings;

namespace ReadyM.Loader.Wukong.Bootstrap;

public class DI
{
    public static readonly DI Instance = new();

    private FirstStageDI _firstStage = null!;
    
    public MonoBundledAssemblyArray BundledAssemblyArray { get; private set; } = default;
    
    public GlibAllocator Allocator { get; private set; } = null!;
    public PreprocessAssemblyResolver PreprocessAssemblyResolver { get; private set; } = null!;
    public CompileTimeWeaverBackend CompileTimeBackend { get; private set; } = null!;
    public CompileTimePrelude CompileTimePrelude { get; private set; } = null!;
    public AssemblyPreprocessor AssemblyPreprocessor { get; private set; } = null!;
    
    public ModLocator ModLocator { get; private set; } = null!;
    public ModRegistry ModRegistry { get; private set; } = null!;

    public LoaderFlags LoaderFlags { get; private set; } = null!;

    public LoggerFactoryProvider LoggerProvider
        => _firstStage.LoggerProvider;

    public CurrentLoadingState CurrentLoadingState
        => _firstStage.CurrentLoadingState;

    public PathSettings PathSettings
        => _firstStage.PathSettings;
    
    public ILogger BootstrapLogger
        => _firstStage.BootstrapLogger;

    public unsafe void Init(FirstStageDI firstStageDI, IntPtr bundledAssemblyArrayPtr, IntPtr glibNew0Ptr)
    {
        _firstStage = firstStageDI;
        
        firstStageDI.BootstrapLogger.LogDebug("DI Init started");
        
        var allocator = Allocator = new GlibAllocator(glibNew0Ptr);

        var flags = LoaderFlags = new LoaderFlags(PathSettings);

        if (!flags.PatchGameAssemblies)
        {
            BootstrapLogger.LogWarning("PatchGameAssemblies=0 in b1cs.ini. The game's own assemblies will NOT be");
            BootstrapLogger.LogWarning("patched, so WukongMP will not function. This is a diagnostic mode only.");
        }

        var modLocator = ModLocator = new ModLocator(PathSettings, BootstrapLogger);
        var modRegistry = ModRegistry = modLocator.LocateMods();

        var compileTimeLogger = firstStageDI.LoggerFactory.CreateLogger("CompileTime");

        var bundledAssemblyArray = BundledAssemblyArray = new MonoBundledAssemblyArray((MonoBundledAssembly***)bundledAssemblyArrayPtr);
        var preprocessAssemblyResolver = PreprocessAssemblyResolver = new PreprocessAssemblyResolver(bundledAssemblyArray, allocator, PathSettings, modRegistry, flags, compileTimeLogger);
        var compileTimeBackend = CompileTimeBackend = new CompileTimeWeaverBackend(compileTimeLogger);
        var compileTimePrelude = CompileTimePrelude = new CompileTimePrelude(compileTimeBackend, compileTimeLogger);
        var assemblyPreprocessor = AssemblyPreprocessor = new AssemblyPreprocessor(preprocessAssemblyResolver, compileTimePrelude, compileTimeLogger);
    }
}