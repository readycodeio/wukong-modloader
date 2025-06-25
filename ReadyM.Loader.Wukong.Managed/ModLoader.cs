using System.Reflection;
using System.Text.RegularExpressions;
using CSharpModBase;
using CSharpModBase.Input;
using IniParser;
using Mono.Cecil;
using ReadyM.Loader.Wukong.Bootstrap;
using ReadyM.Loader.Wukong.Managed.Debugger;
using UnrealEngine.Engine;
using Log = ReadyM.Loader.Wukong.Bootstrap.Log;

namespace ReadyM.Loader.Wukong.Managed;

public class ModLoader
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
    }

    private List<ModObject> Mods { get; } = new();
    private List<ModObject> ModsPatched { get; } = new();
    private List<ModObject> ModsInitialized { get; } = new();
    
    private InputManager InputManager { get; } = new();
    
    private Thread? _logLoopThread;
    private Thread? _inputLoopThread;
    private int _reloadCounter;

    public void SetupDefault()
    {
        Log.Debug("Setting up ModLoader");

        SetupModeFromIniConfig();
        SetupModFolderOverride();
    }

    private void SetupModeFromIniConfig()
    {
        var developValue = "0";
        try
        {
            var iniPath = Path.Combine(ModLoaderSettings.LoaderDir, "b1cs.ini");
            var fullIniPath = Path.GetFullPath(iniPath);
            var iniParser = new FileIniDataParser();
            var iniData = iniParser.ReadFile(fullIniPath);
            developValue = iniData["Settings"]?.GetKeyData("Develop")?.Value ?? "0";
        }
        catch (Exception e)
        {
            Log.Error("Error while parsing b1cs.ini");
            Log.Error(e);
        }
        
        ModLoaderSettings.UseDevelop = developValue == "1" || developValue == "2";
        ModLoaderSettings.UseReload = developValue == "2";

        Log.Debug($"Develop Mode: {ModLoaderSettings.UseDevelop}");
        Log.Debug($"Reload Mode: {ModLoaderSettings.UseReload}");
    }

    private void SetupModFolderOverride()
    {
        try
        {
            var cmd = USystemLibrary.GetCommandLine();
            const string pattern = """[a-zA-Z]:\\(?:[^<>:"/\\|?*]+\\)*[^<>:"/\\|?*]*""";
            var pathMatch = Regex.Match(cmd, $"""-mod_folder "?({pattern})"?""");

            if (pathMatch.Success)
            {
                ModLoaderSettings.ModDirSuffix = pathMatch.Groups[1].Value;
                Log.Debug($"Mod folder override: {ModLoaderSettings.ModDir}");
            }
            else
            {
                Log.Debug($"Mod folder: {ModLoaderSettings.ModDir}");
            }
        }
        catch (Exception e)
        {
            Log.Error($"Resolve mod path failed: {e}");
            throw;
        }
    }

    public void LoadMods()
    {
        foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
        {
            Log.Debug($"Already loaded: {asm.FullName}");
        }
        
        Mods.Clear();
        ModsInitialized.Clear();
        if (!Directory.Exists(ModLoaderSettings.ModDir))
        {
            Log.Error($"Mod dir {ModLoaderSettings.ModDir} not exists");
            return;
        }

        var allDirs = Directory.GetDirectories(ModLoaderSettings.ModDir);
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
                Log.Debug($"Default order: {dir}");
            else
                Log.Debug($"Order: {loadOrder} {dir}");

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

        var copyHelper = new AssemblyCopyHelper();
        var renameHelper = new AssemblyRenameHelper();

        ModLoaderSettings.CloneDir = copyHelper.GetTempPath();
        copyHelper.SetReloadSuffix($"__{_reloadCounter++}");

        Log.Debug("======== Marking develop assemblies ========");

        foreach (var d in meta)
        {
            var dir = d.Key;
            var modMeta = d.Value;
            var modName = modMeta.ModName;

            ModLoaderSettings.LoadingModName = modName;

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

                if (ModLoaderSettings.UseReload)
                {
                    copyHelper.MarkForReload(
                        renameHelper,
                        asmPath,
                        out var asmName,
                        out var renamedAsmName,
                        out var asmFullName,
                        out var renamedAsmFullName
                    );
                    Log.Debug($"Marked for reload: {asmFullName} -> {renamedAsmFullName}");
                }
            }

            modMeta.OrigAsmPath = Path.Combine(dir, $"{modName}.dll");
        }

        Log.Debug("======== Copying assemblies ========");

        foreach (var d in meta)
        {
            var dir = d.Key;
            var modMeta = d.Value;
            var modName = modMeta.ModName;
            
            Log.Debug($"Processing {modName}");

            if (modMeta.Disabled)
            {
                Log.Debug("Mod disabled");
                continue;
            }
            
            var resolver = new DefaultAssemblyResolver();
            resolver.AddSearchDirectory(dir);
            resolver.AddSearchDirectory(Path.Combine(ModLoaderSettings.ModDir, "Common"));
            resolver.AddSearchDirectory(Path.Combine(ModLoaderSettings.ModDir, "ReflectionOnly"));
            if (ModLoaderSettings.CloneDir != null)
            {
                resolver.AddSearchDirectory(Path.Combine(ModLoaderSettings.CloneDir, modName));
                resolver.AddSearchDirectory(Path.Combine(ModLoaderSettings.CloneDir, "Common"));
                resolver.AddSearchDirectory(Path.Combine(ModLoaderSettings.CloneDir, "ReflectionOnly"));
            }

            resolver.AddSearchDirectory(ModLoaderSettings.LoaderDir);

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
                if (ModLoaderSettings.UseReload)
                {
                    copyHelper.CreateAssemblyClone(
                        asmPath,
                        modName,
                        out copiedAsmPath,
                        out copiedAsmSymbols,
                        renameHelper,
                        resolver
                    );
                    
                    Log.Debug($"Copied {asmPath} -> {copiedAsmPath}");
                    if (asmSymbols != null)
                        Log.Debug($"Copied {asmSymbols} -> {copiedAsmSymbols}");
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
        foreach (var d in meta)
        {
            var dir = d.Key;
            var modMeta = d.Value;
            var modName = modMeta.ModName;

            if (modMeta.Disabled)
                continue;
            
            if (modMeta.LoadAsmPath == null)
            {
                Log.Debug($"No assembly to load for: {modName}");
                continue;
            }

            ModLoaderSettings.LoadingModName = modName;

            try
            {
                Log.Debug($"======== Loading {modMeta.LoadAsmPath} ========");

                LoadResourceDlls(dir);
                var asm = Assembly.LoadFrom(modMeta.LoadAsmPath);
                Log.Debug($"Loaded: {modMeta.LoadAsmPath}");

                foreach (var type in asm.GetTypes())
                {
                    if (csharpModType.IsAssignableFrom(type))
                    {
                        if (csharpModExType.IsAssignableFrom(type))
                            Log.Debug($"Found {nameof(ICSharpModEx)}: {type}");
                        else
                            Log.Debug($"Found {nameof(ICSharpMod)}: {type}");

                        if (Activator.CreateInstance(type) is ICSharpMod mod)
                        {
                            var modObj = new ModObject()
                            {
                                Meta = modMeta,
                                Mod = mod,
                                ModEx = mod as ICSharpModEx
                            };
                            Mods.Add(modObj);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Error($"Loading {modMeta.LoadAsmPath} failed:");
                Log.Error(ex);
            }
            
            ModLoaderSettings.LoadingModName = null;
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
                Log.Debug($"Loaded resource: {resourceAsm.FullName}");
            }
        }
        catch (Exception ex)
        {
            Log.Error($"Loading resources from {dir} failed:");
            Log.Error(ex);
        }
    }

    /*
    public void PatchMods()
    {
        foreach (var modObj in Mods)
        {
            ModLoaderSettings.LoadingModName = modObj.Meta.ModName;

            try
            {
                if (modObj.ModEx != null)
                {
                    modObj.ModEx.Patch();
                    ModsPatched.Add(modObj);
                    Log.Debug($"Patched: {modObj.Mod.Name} ({modObj.Mod.Version})");
                }
            }
            catch (Exception ex)
            {
                Log.Debug($"Patching {modObj.Mod.Name} ({modObj.Mod.Version}) failed:");
                Log.Error(ex);
            }
            
            ModLoaderSettings.LoadingModName = null;
        }
    }

    public void UnpatchMods()
    {
        var modsPatched = new List<ModObject>(ModsPatched);
        modsPatched.Reverse();
        foreach (var modObj in modsPatched)
        {
            ModLoaderSettings.LoadingModName = modObj.Meta.ModName;

            try
            {
                if (modObj.ModEx != null)
                {
                    modObj.ModEx.Patch();
                    ModsPatched.Remove(modObj);
                    Log.Debug($"Unpatched: {modObj.Mod.Name} ({modObj.Mod.Version})");
                }
            }
            catch (Exception ex)
            {
                Log.Error($"Unpatching {modObj.Mod.Name} ({modObj.Mod.Version}) failed:");
                Log.Error(ex);
            }
            
            ModLoaderSettings.LoadingModName = null;
        }
    }
    */

    public void InitMods(bool reload, Dictionary<string, object>? reloadContexts = null)
    {
        foreach (var modObj in Mods)
        {
            ModLoaderSettings.LoadingModName = modObj.Meta.ModName;

            try
            {
                modObj.Mod.Init();
                ModsInitialized.Add(modObj);
                Log.Debug($"Initialized: {modObj.Mod.Name} ({modObj.Mod.Version})");
                                
                if (reload && modObj.ModEx != null)
                {
                    reloadContexts!.TryGetValue(modObj.ModEx.Name, out var reloadContext);
                    modObj.ModEx.Reload(reloadContext);
                    Log.Debug($"Reloaded: {modObj.Mod.Name} ({modObj.Mod.Version})");
                }
            }
            catch (Exception ex)
            {
                Log.Error($"Initializing {modObj.Mod.Name} ({modObj.Mod.Version}) failed:");
                Log.Error(ex);
            }
            
            ModLoaderSettings.LoadingModName = null;
        }
    }

    public void DeinitMods()
    {
        var modsInitialized = new List<ModObject>(ModsInitialized);
        modsInitialized.Reverse();
        foreach (var modObj in modsInitialized)
        {
            ModLoaderSettings.LoadingModName = modObj.Meta.ModName;

            try
            {
                modObj.Mod.Init();
                ModsInitialized.Add(modObj);
                Log.Debug($"Deinitialized: {modObj.Mod.Name} ({modObj.Mod.Version})");
            }
            catch (Exception ex)
            {
                Log.Error($"Deinitializing {modObj.Mod.Name} ({modObj.Mod.Version}) failed:");
                Log.Error(ex);
            }
            
            ModLoaderSettings.LoadingModName = null;
        }
    }

    public Dictionary<string, object> GetReloadContexts()
    {
        var result = new Dictionary<string, object>();
        foreach (var modObj in ModsInitialized)
        {
            try
            {
                if (modObj.ModEx != null)
                {
                    var reloadContext = modObj.ModEx.GetReloadContext();
                    Log.Debug($"Reload context for: {modObj.Mod.Name} ({modObj.Mod.Version})");
                    if (reloadContext != null)
                        result.Add(modObj.ModEx.Name, reloadContext);
                }
            }
            catch (Exception ex)
            {
                Log.Error($"Fetching reload context {modObj.Mod.Name} ({modObj.Mod.Version}) failed:");
                Log.Error(ex);
            }
        }
        
        return result;
    }

    public void ReloadMods()
    {
        Log.Debug("Fetching reload contexts");
        var reloadContexts = GetReloadContexts();
        
        Log.Debug("Reloading mods");
        InputManager.Clear();
        
        DeinitMods();
        //UnpatchMods();
        
        LoadMods();
        
        //PatchMods();
        InitMods(true, reloadContexts);
    }

    public void StartLogLoop()
    {
        _logLoopThread = new Thread(LogLoop)
        {
            IsBackground = true,
        };
        _logLoopThread.Start();
    }

    private static void LogLoop()
    {
        while (true)
        {
            Log.Flush();
            Thread.Sleep(100);
        }
    }

    public void StartInputLoop()
    {
        Utils.InitInputManager(InputManager);

        InputManager.RegisterBuiltinKeyBind(ModifierKeys.Control, Key.F5, ReloadMods);
        _inputLoopThread = new Thread(InputLoop)
        {
            IsBackground = true,
        };
        _inputLoopThread.Start();
    }

    private void InputLoop()
    {
        while (true)
        {
            InputManager.Update();
            Thread.Sleep(10); // 10ms
        }
    }
}
