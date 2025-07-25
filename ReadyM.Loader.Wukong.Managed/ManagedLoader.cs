using System.Reflection;
using CSharpManager;
using CSharpModBase;
using CSharpModBase.Input;
using EmbedCSharpLoader.Managed;
using IniParser;
using Microsoft.Extensions.Logging;
using Mono.Cecil;
using ReadyM.Loader.Wukong.Bootstrap;
using ReadyM.Loader.Wukong.Managed.Debugger;
using Log = ReadyM.Loader.Wukong.Bootstrap.Log;

namespace ReadyM.Loader.Wukong.Managed;

public class ManagedLoader
{
    private class ModMetadata
    {
        public string ModName;
        public int LoadOrder;
        public string? OrigAsmPath;
        public string? LoadAsmPath;
        public bool Disabled;
    }

    private class ModObject
    {
        public ModMetadata Meta;
        public ICSharpMod Mod;
        public ICSharpModEx? ModEx;
        public ICSharpModExV2? ModExV2;
    }

    private readonly ILogger _logger;
    
    private List<ModObject> _mods { get; } = new();
    private List<ModObject> _modsInitialized { get; } = new();
    private List<ModObject> _modsLateInitialized { get; } = new();
    
    private InputManager _inputManager { get; } = new();

    private readonly CancellationTokenSource _cancellationTokenSource = new();
    private CancellationToken _cancellationToken;
    private Thread? _lateInitThread;

    private Thread? _logLoopThread;
    private Thread? _inputLoopThread;
    private int _reloadCounter;
    private readonly IpcHelper _ipcHelper;

    public ManagedLoader(ILogger logger, IpcHelper ipcHelper)
    {
        _logger = logger;
        _ipcHelper = ipcHelper;
        _cancellationToken = _cancellationTokenSource.Token;
    }

    public void SetupDefault()
    {
        _logger.LogDebug("Setting up ModLoader");

        SetupModeFromIniConfig();
        SetupModFolderOverride();
    }

    private void SetupModeFromIniConfig()
    {
        var developValue = "0";
        try
        {
            var iniPath = Path.Combine(Settings.LoaderDir, "b1cs.ini");
            var fullIniPath = Path.GetFullPath(iniPath);
            var iniParser = new FileIniDataParser();
            var iniData = iniParser.ReadFile(fullIniPath);
            developValue = iniData["Settings"]?.GetKeyData("Develop")?.Value ?? "0";
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error while parsing b1cs.ini");
        }
        
        Settings.UseDevelop = developValue == "1" || developValue == "2";
        Settings.UseReload = developValue == "2";

        _logger.LogDebug("Develop Mode: {DevelopMode}", Settings.UseDevelop);
        _logger.LogDebug("Reload Mode: {ReloadMode}", Settings.UseReload);
    }

    private void SetupModFolderOverride()
    {
        try
        {
            var ipcHandshakeFile = _ipcHelper.ReadIpcHandshakeFile();

            if (ipcHandshakeFile.TryGetValue("MOD_FOLDER", out var modFolder))
            {
                Settings.ModDirOverride = modFolder;
                _logger.LogDebug("Mod folder override: {Path}", Settings.ModDir);
            }
            else
            {
                _logger.LogDebug("Mod folder: {Path}", Settings.ModDir);
            }
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Resolve mod path failed:");
            throw;
        }
    }

    public void LoadMods()
    {
        foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
        {
            _logger.LogDebug("Already loaded: {AssemblyName}", asm.FullName);
        }
        
        _mods.Clear();
        _modsInitialized.Clear();
        if (!Directory.Exists(Settings.ModDir))
        {
            _logger.LogError("Mod dir {Path} not exists", Settings.ModDir);
            return;
        }

        var allDirs = Directory.GetDirectories(Settings.ModDir);
        var dirs = new List<string>();
        foreach (var dir in allDirs)
        {
            var modName = Path.GetFileName(dir);
            if (modName == "Common" || modName == "ReflectionOnly" || modName == "Overrides")
                continue;

            dirs.Add(dir);
        }

        var meta = new Dictionary<string, ModMetadata>();
        foreach (var dir in dirs)
        {
            var disabledPath = Path.Combine(dir, "disabled.txt");
            var orderPath = Path.Combine(dir, "order.txt");
            int? loadOrder = null;
            if (File.Exists(orderPath))
            {
                var orderStr = File.ReadAllText(orderPath).Trim();
                if (int.TryParse(orderStr, out var parsedOrder))
                {
                    loadOrder = parsedOrder;
                }
            }

            if (loadOrder == null)
                _logger.LogDebug("Default order: {Path}", dir);
            else
                _logger.LogDebug("Order: {Order} {Path}", loadOrder, dir);

            var modName = Path.GetFileName(dir);
            var modMeta = new ModMetadata()
            {
                ModName = modName!,
                LoadOrder = loadOrder ?? 0,
                Disabled = File.Exists(disabledPath),
            };

            meta.Add(dir, modMeta);
        }

        dirs.Sort(Comparer<string>.Create((x, y) => meta[x].LoadOrder.CompareTo(meta[y].LoadOrder)));

        var copyHelper = new AssemblyCopyHelper(_logger);
        var renameHelper = new AssemblyRenameHelper();

        Settings.CloneDir = copyHelper.GetTempPath();
        copyHelper.SetReloadSuffix($"__{_reloadCounter++}");

        _logger.LogDebug("======== Marking develop assemblies ========");

        foreach (var d in meta)
        {
            var dir = d.Key;
            var modMeta = d.Value;
            var modName = modMeta.ModName;

            Settings.LoadingModName = modName;

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

                if (Settings.UseReload)
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

            modMeta.OrigAsmPath = Path.Combine(dir, $"{modName}.dll");
        }

        _logger.LogDebug("======== Copying assemblies ========");

        foreach (var d in meta)
        {
            var dir = d.Key;
            var modMeta = d.Value;
            var modName = modMeta.ModName;
            
            _logger.LogDebug("Processing {Name}", modName);

            if (modMeta.Disabled)
            {
                _logger.LogDebug("Mod disabled");
                continue;
            }

            var resolver = new DefaultAssemblyResolver();
            resolver.AddSearchDirectory(dir);
            resolver.AddSearchDirectory(Path.Combine(Settings.ModDir, "Common"));
            resolver.AddSearchDirectory(Path.Combine(Settings.ModDir, "ReflectionOnly"));
            if (Settings.CloneDir != null)
            {
                resolver.AddSearchDirectory(Path.Combine(Settings.CloneDir, modName));
                resolver.AddSearchDirectory(Path.Combine(Settings.CloneDir, "Common"));
                resolver.AddSearchDirectory(Path.Combine(Settings.CloneDir, "ReflectionOnly"));
            }

            resolver.AddSearchDirectory(Settings.LoaderDir);

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
                if (Settings.UseReload)
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

                if (asmPath == modMeta.OrigAsmPath)
                    modMeta.LoadAsmPath = copiedAsmPath;
            }
        }

        var csharpModType = typeof(ICSharpMod);
        var csharpModExType = typeof(ICSharpModEx);
        var csharpModExV2Type = typeof(ICSharpModExV2);
        
        foreach (var d in meta)
        {
            var dir = d.Key;
            var modMeta = d.Value;
            var modName = modMeta.ModName;

            if (modMeta.Disabled)
                continue;

            if (modMeta.LoadAsmPath == null)
            {
                _logger.LogDebug("No assembly to load for: {Name}", modName);
                continue;
            }

            Settings.LoadingModName = modName;

            try
            {
                _logger.LogTrace("======== Loading {Path} ========", modMeta.LoadAsmPath);

                LoadResourceDlls(dir);
                var asm = Assembly.LoadFrom(modMeta.LoadAsmPath);
                _logger.LogTrace("Loaded: {Path}", modMeta.LoadAsmPath);

                foreach (var type in asm.GetTypes())
                {
                    if (csharpModType.IsAssignableFrom(type))
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

                        var mod = modUntyped as ICSharpMod;
                        if (mod == null)
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
                        
                        var modObj = new ModObject()
                        {
                            Meta = modMeta,
                            Mod = mod,
                            ModEx = mod as ICSharpModEx,
                            ModExV2 = mod as ICSharpModExV2,
                        };
                        _mods.Add(modObj);
                    }
                }
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Loading {Path} failed:", modMeta.LoadAsmPath);
            }
            
            Settings.LoadingModName = null;
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
        foreach (var modObj in _mods)
        {
            Settings.LoadingModName = modObj.Meta.ModName;

            try
            {
                modObj.Mod.Init();
                Log.Provider.Flush();
                _modsInitialized.Add(modObj);
                _logger.LogDebug("Initialized: {Name} ({Version})", modObj.Mod.Name, modObj.Mod.Version);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Initializing {Name} ({Version}) failed:", modObj.Mod.Name, modObj.Mod.Version);
            }
            
            Settings.LoadingModName = null;
        }
    }

    public void LateInitMods(bool reload, Dictionary<string, object>? reloadContexts = null)
    {
        foreach (var modObj in _mods)
        {
            if (_cancellationToken.IsCancellationRequested)
                break;

            Settings.LoadingModName = modObj.Meta.ModName;

            try
            {
                if (!_modsInitialized.Contains(modObj))
                {
                    _logger.LogWarning("Skipping late init for not initialized mod: {Name} ({Version})", modObj.Mod.Name, modObj.Mod.Version);
                    continue;
                }

                if (modObj.ModExV2 != null)
                {
                    modObj.ModExV2.LateInit();
                    Log.Provider.Flush();
                    _modsLateInitialized.Add(modObj);
                    _logger.LogDebug("Late Initialized: {Name} ({Version})", modObj.Mod.Name, modObj.Mod.Version);
                }

                if (reload && modObj.ModEx != null)
                {
                    reloadContexts!.TryGetValue(modObj.ModEx.Name, out var reloadContext);
                    modObj.ModEx.Reload(reloadContext);
                    Log.Provider.Flush();
                    _logger.LogDebug("Reloaded: {Name} ({Version})", modObj.Mod.Name, modObj.Mod.Version);
                }
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Initializing {Name} ({Version}) failed:", modObj.Mod.Name, modObj.Mod.Version);
            }
            
            Settings.LoadingModName = null;
        }
    }

    public void DeInitMods()
    {
        _lateInitThread?.Join();
        
        var modsInitialized = new List<ModObject>(_modsInitialized);
        modsInitialized.Reverse();
        foreach (var modObj in modsInitialized)
        {
            Settings.LoadingModName = modObj.Meta.ModName;

            try
            {
                modObj.Mod.DeInit();
                Log.Provider.Flush();
                _modsInitialized.Remove(modObj);
                _logger.LogDebug("Deinitialized: {Name} ({Version})", modObj.Mod.Name, modObj.Mod.Version);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Deinitializing {Name} ({Version}) failed:", modObj.Mod.Name, modObj.Mod.Version);
            }
            
            Settings.LoadingModName = null;
        }
    }

    public Dictionary<string, object> GetReloadContexts()
    {
        var result = new Dictionary<string, object>();
        foreach (var modObj in _modsInitialized)
        {
            try
            {
                if (modObj.ModEx != null)
                {
                    var reloadContext = modObj.ModEx.GetReloadContext();
                    Log.Provider.Flush();
                    _logger.LogDebug("Reload context for: {Name} ({Version})", modObj.Mod.Name, modObj.Mod.Version);
                    if (reloadContext != null)
                        result.Add(modObj.ModEx.Name, reloadContext);
                }
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Fetching reload context {Name} ({Version}) failed:", modObj.Mod.Name, modObj.Mod.Version);
            }
        }

        return result;
    }

    public void ReloadMods()
    {
        _lateInitThread?.Join();
        
        _logger.LogDebug("Fetching reload contexts");
        var reloadContexts = GetReloadContexts();
        
        _logger.LogDebug("Reloading mods");
        _inputManager.Clear();
        
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
                LateInitMods(false, null);
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

    public void StartLogLoop()
    {
        _logLoopThread = new Thread(LogLoop)
        {
            IsBackground = true,
        };
        _logLoopThread.Start();
    }

    private void LogLoop()
    {
        while (!_cancellationToken.IsCancellationRequested)
        {
            Log.Provider.Flush();
            _cancellationToken.WaitHandle.WaitOne(100);
        }
    }

    public void StartInputLoop()
    {
        Utils.InitInputManager(_inputManager);

        _inputManager.RegisterBuiltinKeyBind(ModifierKeys.Control, Key.F5, ReloadMods);
        _inputLoopThread = new Thread(InputLoop)
        {
            IsBackground = true,
        };
        _inputLoopThread.Start();
    }

    private void InputLoop()
    {
        while (!_cancellationToken.IsCancellationRequested)
        {
            _inputManager.Update();
            _cancellationToken.WaitHandle.WaitOne(10);
        }
    }

    public void Cancel()
    {
        _logger.LogDebug("Cancelling in progress operations...");
        _cancellationTokenSource.Cancel();

        _lateInitThread?.Join();
        _logLoopThread?.Join();
        _inputLoopThread?.Join();
        
        _logger.LogDebug("All operations cancelled.");
    }
}