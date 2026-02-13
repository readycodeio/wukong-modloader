using System.Reflection;
using CSharpModBase;
using Microsoft.Extensions.Logging;
using Mono.Cecil;
using ReadyM.Loader.Wukong.Bootstrap.Registry;
using ReadyM.Loader.Wukong.Bootstrap.Settings;
using ReadyM.Loader.Wukong.Managed.Debugger;
using Log = ReadyM.Loader.Wukong.Bootstrap.Log;

namespace ReadyM.Loader.Wukong.Managed;

public class ModLoader
{
    private class ModLoadState
    {
        public string? LoadAsmPath;
        public ICSharpMod Mod;
        public ICSharpModEx? ModEx;
        public ICSharpModExV2? ModExV2;
    }

    private readonly ModRegistry _modRegistry;
    private readonly LoadingPhaseManager _loadingPhaseManager;
    private readonly CurrentLoadingState _currentLoadingState;
    private readonly PathSettings _pathSettings;
    private readonly ModLoaderSettings _modLoaderSettings;
    private readonly ILogger _logger;

    private readonly Dictionary<string, ModLoadState> _modLoadState = [];

    private readonly List<string> _modsInitialized = [];
    private readonly List<string> _modsLateInitialized = [];

    private Thread? _lateInitThread;
    private int _reloadCounter;

    public ModLoader(
        ModRegistry modRegistry,
        LoadingPhaseManager loadingPhaseManager,
        CurrentLoadingState currentLoadingState,
        PathSettings pathSettings,
        ModLoaderSettings modLoaderSettings,
        ILogger logger
    )
    {
        _modRegistry = modRegistry;
        _loadingPhaseManager = loadingPhaseManager;
        _currentLoadingState = currentLoadingState;
        _pathSettings = pathSettings;
        _modLoaderSettings = modLoaderSettings;
        _logger = logger;

        _loadingPhaseManager.OnCancel += OnCancel;
    }

    public void LoadMods()
    {
        foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
        {
            _logger.LogDebug("Already loaded: {AssemblyName}", asm.FullName);
        }
        
        _modLoadState.Clear();
        _modsInitialized.Clear();
        
        if (!Directory.Exists(_pathSettings.ModDir))
        {
            _logger.LogError("Mod dir {Path} not exists", _pathSettings.ModDir);
            return;
        }

        var copyHelper = new AssemblyCopyHelper(_logger);
        var renameHelper = new AssemblyRenameHelper();

        _currentLoadingState.CloneDir = copyHelper.GetTempPath();
        copyHelper.SetReloadSuffix($"__{_reloadCounter++}");

        _logger.LogDebug("======== Marking develop assemblies ========");

        foreach (var dir in _modRegistry.ModDirs)
        {
            var modMeta = _modRegistry.MetaByDir[dir];
            var modName = modMeta.ModName;

            _currentLoadingState.LoadingModName = modName;

            foreach (var f in Directory.GetFiles(dir))
            {
                if (!f.EndsWith(".dll"))
                    continue;
                if (f.EndsWith(".32.dll") || f.EndsWith(".64.dll") ||
                    f.EndsWith(".x86.dll") || f.EndsWith(".x64.dll") ||
                    f.EndsWith("-32.dll") || f.EndsWith("-64.dll") ||
                    f.EndsWith("-x86.dll") || f.EndsWith("-x64.dll"))
                    continue;

                var asmPath = f;

                if (_modLoaderSettings.UseReload)
                {
                    copyHelper.MarkForReload(
                        renameHelper,
                        asmPath,
                        out var asmName,
                        out var renamedAsmName,
                        out var asmFullName,
                        out var renamedAsmFullName
                    );
                    _logger.LogDebug("Marked for reload: {OldName} -> {NewName}", asmFullName, renamedAsmFullName);
                }
            }
        }

        _logger.LogDebug("======== Copying assemblies ========");

        foreach (var dir in _modRegistry.ModDirs)
        {
            var modMeta = _modRegistry.MetaByDir[dir];
            var modName = modMeta.ModName;

            _logger.LogDebug("Processing {Name}", modName);
            var modLoadState = new ModLoadState();
            _modLoadState.Add(dir, modLoadState);

            if (modMeta.Disabled)
            {
                _logger.LogDebug("Mod disabled");
                continue;
            }

            var resolver = new DefaultAssemblyResolver();
            resolver.AddSearchDirectory(dir);
            resolver.AddSearchDirectory(Path.Combine(_pathSettings.ModDir, "Common"));
            resolver.AddSearchDirectory(Path.Combine(_pathSettings.ModDir, "ReflectionOnly"));
            if (_currentLoadingState.CloneDir != null)
            {
                resolver.AddSearchDirectory(Path.Combine(_currentLoadingState.CloneDir, modName));
                resolver.AddSearchDirectory(Path.Combine(_currentLoadingState.CloneDir, "Common"));
                resolver.AddSearchDirectory(Path.Combine(_currentLoadingState.CloneDir, "ReflectionOnly"));
            }

            resolver.AddSearchDirectory(_pathSettings.LoaderDir);

            foreach (var f in Directory.GetFiles(dir))
            {
                if (!f.EndsWith(".dll"))
                    continue;
                if (f.EndsWith(".32.dll") || f.EndsWith(".64.dll") ||
                    f.EndsWith(".x86.dll") || f.EndsWith(".x64.dll") ||
                    f.EndsWith("-32.dll") || f.EndsWith("-64.dll") ||
                    f.EndsWith("-x86.dll") || f.EndsWith("-x64.dll"))
                    continue;

                var asmPath = f;

                var asmSymbols = AssemblyCopyHelper.RealChangeExtension(asmPath, "pdb");
                if (!File.Exists(asmSymbols))
                    asmSymbols = null;

                string copiedAsmPath;
                string? copiedAsmSymbols;
                if (_modLoaderSettings.UseReload)
                {
                    copyHelper.CreateAssemblyClone(
                        asmPath,
                        modName,
                        out copiedAsmPath,
                        out copiedAsmSymbols,
                        renameHelper,
                        resolver
                    );
                    
                    _logger.LogDebug("Copied {Path} -> {CopyPath}", asmPath, copiedAsmPath);
                    if (asmSymbols != null)
                        _logger.LogDebug("Copied {Path} -> {CopyPath}", asmSymbols, copiedAsmSymbols);
                }
                else
                {
                    copiedAsmPath = asmPath;
                }

                if (asmPath == modMeta.MainAsmPath)
                    modLoadState.LoadAsmPath = copiedAsmPath;
            }
        }

        var csharpModType = typeof(ICSharpMod);
        var csharpModExType = typeof(ICSharpModEx);
        var csharpModExV2Type = typeof(ICSharpModExV2);
        
        foreach (var dir in _modRegistry.ModDirs)
        {
            var modMeta = _modRegistry.MetaByDir[dir];
            var modName = modMeta.ModName;
            var modLoadState = _modLoadState[dir];

            if (modMeta.Disabled)
                continue;

            if (modLoadState.LoadAsmPath == null)
            {
                _logger.LogDebug("No assembly to load for: {Name}", modName);
                continue;
            }

            _currentLoadingState.LoadingModName = modName;

            try
            {
                _logger.LogTrace("======== Loading {Path} ========", modLoadState.LoadAsmPath);

                LoadResourceDlls(dir);
                var asm = Assembly.LoadFrom(modLoadState.LoadAsmPath);
                _logger.LogTrace("Loaded: {Path}", modLoadState.LoadAsmPath);

                foreach (var type in asm.GetTypes())
                {
                    if (csharpModType.IsAssignableFrom(type) && type is { IsAbstract: false, IsInterface: false })
                    {
                        var baseType = csharpModExV2Type.IsAssignableFrom(type) ? csharpModExV2Type :
                            csharpModExType.IsAssignableFrom(type) ? csharpModExType : csharpModType;

                        _logger.LogTrace("Found {BaseType}: {Type}", baseType, type);

                        var modUntyped = Activator.CreateInstance(type);
                        if (modUntyped == null)
                        {
                            _logger.LogError("Failed to create instance of {TypeName}", type.FullName);
                            continue;
                        }

                        if (modUntyped is not ICSharpMod mod)
                        {
                            _logger.LogError("Instance of {TypeName} is not ICSharpMod", type.FullName);
                            continue;
                        }

                        if (mod is ICSharpModExV2 modExV2)
                        {
                            var loggerFactory = Log.Provider.CreateLoggerFactory(modExV2.IsDebug, false);
                            modExV2.SetLoggerFactory(loggerFactory);
                            Log.Provider.Flush();
                        }

                        modLoadState.Mod = mod;
                        modLoadState.ModEx = mod as ICSharpModEx;
                        modLoadState.ModExV2 = mod as ICSharpModExV2;
                    }
                }
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Loading {Path} failed:", modLoadState.LoadAsmPath);
            }
            
            _currentLoadingState.LoadingModName = null;
        }
    }

    private void LoadResourceDlls(string dir)
    {
        try
        {
            string[] resourcePaths = Directory.GetFiles(dir, "*.resources.dll", SearchOption.AllDirectories);
            foreach (var resourcePath in resourcePaths)
            {
                var resourceAsm = Assembly.LoadFrom(resourcePath);
                _logger.LogDebug("Loaded resource: {Name}", resourceAsm.FullName);
            }
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Loading resources from {Path} failed:", dir);
        }
    }

    public void InitMods()
    {
        foreach (var dir in _modRegistry.ModDirs.Reverse()) // TODO: Topological sort
        {
            if (!_modLoadState.TryGetValue(dir, out var modLoadState))
                continue;
            
            if (modLoadState.LoadAsmPath is null)
                continue;
            
            var modMeta = _modRegistry.MetaByDir[dir];
            _currentLoadingState.LoadingModName = modMeta.ModName;

            try
            {
                modLoadState.Mod.Init();
                Log.Provider.Flush();
                _modsInitialized.Add(dir);
                _logger.LogDebug("Initialized: {Name} ({Version})", modLoadState.Mod.Name, modLoadState.Mod.Version);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Initializing {Name} ({Version}) failed:", modLoadState.Mod.Name, modLoadState.Mod.Version);
                // log inner exceptions
                var innerEx = ex.InnerException;
                while (innerEx != null)
                {
                    _logger.LogError(innerEx, "Inner exception:");
                    innerEx = innerEx.InnerException;
                }
            }
            
            _currentLoadingState.LoadingModName = null;
        }
    }

    public void LateInitMods(bool reload, Dictionary<string, object>? reloadContexts = null)
    {
        foreach (var dir in _modRegistry.ModDirs)
        {
            if (_loadingPhaseManager.IsLoadingCancelled)
                break;
            
            var modMeta = _modRegistry.MetaByDir[dir];
            if (!_modLoadState.TryGetValue(dir, out var modLoadState))
                continue;
            
            if (modLoadState.LoadAsmPath is null)
                continue;
            

            _currentLoadingState.LoadingModName = modMeta.ModName;

            try
            {
                if (!_modsInitialized.Contains(dir))
                {
                    _logger.LogWarning("Skipping late init for not initialized mod: {Name} ({Version})", modLoadState.Mod.Name, modLoadState.Mod.Version);
                    continue;
                }

                if (modLoadState.ModExV2 != null)
                {
                    modLoadState.ModExV2.LateInit();
                    Log.Provider.Flush();
                    _modsLateInitialized.Add(dir);
                    _logger.LogDebug("Late Initialized: {Name} ({Version})", modLoadState.Mod.Name, modLoadState.Mod.Version);
                }

                if (reload && modLoadState.ModEx != null)
                {
                    reloadContexts!.TryGetValue(modLoadState.ModEx.Name, out var reloadContext);
                    modLoadState.ModEx.Reload(reloadContext);
                    Log.Provider.Flush();
                    _logger.LogDebug("Reloaded: {Name} ({Version})", modLoadState.Mod.Name, modLoadState.Mod.Version);
                }
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Initializing {Name} ({Version}) failed:", modLoadState.Mod.Name, modLoadState.Mod.Version);
            }
            
            _currentLoadingState.LoadingModName = null;
        }
    }

    public void DeInitMods()
    {
        _lateInitThread?.Join();
        
        var modsInitialized = new List<string>(_modsInitialized);
        modsInitialized.Reverse();
        foreach (var dir in modsInitialized)
        {
            var modMeta = _modRegistry.MetaByDir[dir];
            if (!_modLoadState.TryGetValue(dir, out var modLoadState))
                continue;
            
            _currentLoadingState.LoadingModName = modMeta.ModName;

            try
            {
                modLoadState.Mod.DeInit();
                Log.Provider.Flush();
                _modsInitialized.Remove(dir);
                _logger.LogDebug("Deinitialized: {Name} ({Version})", modLoadState.Mod.Name, modLoadState.Mod.Version);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Deinitializing {Name} ({Version}) failed:", modLoadState.Mod.Name, modLoadState.Mod.Version);
            }
            
            _currentLoadingState.LoadingModName = null;
        }
    }

    public Dictionary<string, object> GetReloadContexts()
    {
        var result = new Dictionary<string, object>();
        foreach (var dir in _modsInitialized)
        {
            if (!_modLoadState.TryGetValue(dir, out var modLoadState))
                continue;
            
            try
            {
                if (modLoadState.ModEx != null)
                {
                    var reloadContext = modLoadState.ModEx.GetReloadContext();
                    Log.Provider.Flush();
                    _logger.LogDebug("Reload context for: {Name} ({Version})", modLoadState.Mod.Name, modLoadState.Mod.Version);
                    if (reloadContext != null)
                        result.Add(modLoadState.ModEx.Name, reloadContext);
                }
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Fetching reload context {Name} ({Version}) failed:", modLoadState.Mod.Name, modLoadState.Mod.Version);
            }
        }

        return result;
    }

    public void ReloadMods()
    {
        _lateInitThread?.Join();
        
        _logger.LogDebug("Fetching reload contexts");
        var reloadContexts = GetReloadContexts();
        
        DeInitMods();
        LoadMods();
        InitMods();
        LateInitMods(true, reloadContexts);
    }

    public void StartLateInitMods()
    {
        _lateInitThread = new Thread(() =>
        {
            _logger.LogDebug("Starting late init thread");
            try
            {
                LateInitMods(false);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Late init mods failed:");
            }
        })
        {
            IsBackground = false,
        };
        _lateInitThread.Start();
    }

    private void OnCancel()
    {
        _lateInitThread?.Join();
    }
}