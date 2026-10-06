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
        /// Null for a mod entered through a [ModEntry] class, which the SDK builds out of its container.
        public ICSharpMod? Mod;
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

    /// Read off the metadata rather than the type, since this runs before the assembly is loaded.
    private const string ModEntryAttributeName = "ReadyM.SDK.Attributes.ModEntryAttribute";

    /// The bases a mod may be entered through, whichever of them it names.
    private static readonly string[] ModBaseNames = ["ModBase", "ModHostBase"];

    /// <summary>
    /// Whether the type descends from one of the bases the loader enters a mod through.
    /// </summary>
    private static bool DerivesFromModBase(TypeDefinition type)
    {
        var baseType = type.BaseType;
        
        for (var depth = 0; baseType != null && depth < 16; depth++)
        {
            if (ModBaseNames.Contains(baseType.Name))
                return true;

            try
            {
                baseType = baseType.Resolve()?.BaseType;
            }
            catch
            {
                return false;
            }
        }

        return false;
    }

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
        // Filled again below, so a reload does not stack a second copy of every mod onto the first.
        LoadedMods.Clear();

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
            var dirName = Path.GetFileName(dir);

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
                resolver.AddSearchDirectory(Path.Combine(_currentLoadingState.CloneDir, dirName));
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

                using var assembly = AssemblyDefinition.ReadAssembly(asmPath, new ReaderParameters
                {
                    ReadingMode = ReadingMode.Deferred,
                    ReadWrite = false,
                    AssemblyResolver = resolver,
                    ReadSymbols = false
                });
                
                var isMod = assembly.MainModule.Types.Any(t
                    => DerivesFromModBase(t)
                       || t.CustomAttributes.Any(a => a.AttributeType.FullName == ModEntryAttributeName));

                if (isMod)
                {
                    modLoadState.LoadAsmPath = copiedAsmPath;
                    _logger.LogInformation("Marked assembly for loading: {Path}", copiedAsmPath);
                }
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

                // `dir` is the real mod folder, never the reload-mode assembly clone. Left here for
                // a mod with no instance to push it into, whose entry point is built later.
                LoadedMods.Add(asm, dir);

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

                            // `dir` is the real mod folder, never the reload-mode assembly clone.
                            modExV2.SetModDirectory(dir);
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
        foreach (var dir in _modRegistry.ModDirs)
        {
            if (!_modLoadState.TryGetValue(dir, out var modLoadState))
                continue;

            if (modLoadState.LoadAsmPath is null)
                continue;

            if (modLoadState.Mod is null)
            {
                _modsInitialized.Add(dir);
                continue;
            }

            var modMeta = _modRegistry.MetaByDir[dir];
            _currentLoadingState.LoadingModName = modMeta.ModName;

            try
            {
                modLoadState.Mod.Init();
                Log.Provider.Flush();
                _modsInitialized.Add(dir);
                _logger.LogDebug("Initialized: {Name}", modLoadState.Mod.Name);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Initializing {Name} failed:", modLoadState.Mod.Name);
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

            if (modLoadState.Mod is null)
                continue;

            _currentLoadingState.LoadingModName = modMeta.ModName;

            try
            {
                if (!_modsInitialized.Contains(dir))
                {
                    _logger.LogWarning("Skipping late init for not initialized mod: {Name}", modLoadState.Mod.Name);
                    continue;
                }

                if (modLoadState.ModExV2 != null)
                {
                    modLoadState.ModExV2.LateInit();
                    Log.Provider.Flush();
                    _modsLateInitialized.Add(dir);
                    _logger.LogDebug("Late Initialized: {Name}", modLoadState.Mod.Name);
                }

                if (reload && modLoadState.ModEx != null)
                {
                    reloadContexts!.TryGetValue(modLoadState.ModEx.Name, out var reloadContext);
                    modLoadState.ModEx.Reload(reloadContext);
                    Log.Provider.Flush();
                    _logger.LogDebug("Reloaded: {Name}", modLoadState.Mod.Name);
                }
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Initializing {Name} failed:", modLoadState.Mod.Name);
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

            if (modLoadState.Mod is null)
            {
                _modsInitialized.Remove(dir);
                continue;
            }

            _currentLoadingState.LoadingModName = modMeta.ModName;

            try
            {
                modLoadState.Mod.DeInit();
                Log.Provider.Flush();
                _modsInitialized.Remove(dir);
                _logger.LogDebug("Deinitialized: {Name}", modLoadState.Mod.Name);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Deinitializing {Name} failed:", modLoadState.Mod.Name);
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
                    _logger.LogDebug("Reload context for: {Name}", modLoadState.ModEx.Name);
                    if (reloadContext != null)
                        result.Add(modLoadState.ModEx.Name, reloadContext);
                }
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Fetching reload context {Name} failed:", modLoadState.Mod.Name);
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