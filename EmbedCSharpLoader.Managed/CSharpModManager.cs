using System.Reflection;
using System.Text.RegularExpressions;
using CSharpModBase;
using CSharpModBase.Input;
using Mono.Cecil;
using UnrealEngine.Engine;

namespace EmbedCSharpLoader.Managed;

public class CSharpModManager
{
    private static string? LoadingModName { get; set; }
    static string? CloneDir { get; set; }

    private List<ICSharpMod> LoadedMods { get; } = new();
    private InputManager InputManager { get; } = new();
    private bool UseDevelop { get; }
    private bool UseReload { get; }
    private Thread? _loopThread;

    private int _reloadCounter;

    static CSharpModManager()
    {
        Log.Debug("Setting up CSharpModManager");
        AppDomain currentDomain = AppDomain.CurrentDomain;
        currentDomain.AssemblyResolve += AssemblyResolve;
        currentDomain.UnhandledException += OnUnhandledException;

        TaskScheduler.UnobservedTaskException += OnUnobservedTaskException;

        foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
        {
            Log.Debug($"Already loaded: {asm.FullName}");
        }

        Task.Run(Log.Loop);
    }

    private static Assembly? TryLoadDll(string path)
    {
        Log.Debug($"Trying to load from: {path}");
        if (File.Exists(path))
        {
            Log.Debug("Success");
            return Assembly.LoadFrom(path);
        }

        return null;
    }

    private static Assembly? AssemblyResolve(object sender, ResolveEventArgs args)
    {
        try
        {
            var dllName = $"{new AssemblyName(args.Name).Name}.dll";

            Assembly? result;
            if (LoadingModName == null)
            {
                // NOTE: This will prevent the assembly from being loaded
                Log.Warn($"AssemblyResolve: {args.Name} but no mod is loading");
                result = TryLoadDll(Path.Combine(Common.ModDir, "Common", dllName)) ??
                         (CloneDir != null ? TryLoadDll(Path.Combine(CloneDir, "Common", dllName)) : null) ??
                         TryLoadDll(Path.Combine(Common.LoaderDir, dllName));
            }
            else
            {
                result = TryLoadDll(Path.Combine(Common.ModDir, LoadingModName, dllName)) ??
                         TryLoadDll(Path.Combine(Common.ModDir, "Common", dllName)) ??
                         (CloneDir != null ? TryLoadDll(Path.Combine(CloneDir, LoadingModName, dllName)) : null) ??
                         (CloneDir != null ? TryLoadDll(Path.Combine(CloneDir, "Common", dllName)) : null) ??
                         TryLoadDll(Path.Combine(Common.LoaderDir, dllName));
            }

            if (result != null)
                return result;
        }
        catch (Exception e)
        {
            Log.Error($"Load assembly {args.Name} failed:");
            Log.Error(e);
        }

        return Assembly.Load(args.Name);
    }

    private static void OnUnhandledException(object sender, UnhandledExceptionEventArgs e)
    {
        Log.Error("UnhandledException:");
        Log.Error((Exception)e.ExceptionObject);
    }

    private static void OnUnobservedTaskException(object sender, UnobservedTaskExceptionEventArgs e)
    {
        Log.Error("UnobservedTaskException:");
        Log.Error(e.Exception);
    }

    public CSharpModManager()
    {
        Utils.InitInputManager(InputManager);
        // load config from ini
        var iniPath = Path.Combine(Common.LoaderDir, "b1cs.ini");
        var fullIniPath = Path.GetFullPath(iniPath);
        Ini iniFile = new(fullIniPath);
        var developValue = iniFile.GetValue("Develop", "Settings", "0").Trim();
        UseDevelop = developValue == "1" || developValue == "2";
        UseReload = developValue == "2";
        Log.Debug($"Develop Mode: {UseDevelop}");
        Log.Debug($"Reload Mode: {UseReload}");
		CheckModFolderOverride();

        // resolve mod path override
        var cmd = USystemLibrary.GetCommandLine();
        var pattern = @"[a-zA-Z]:\\(?:[^<>:""/\\|?*]+\\)*[^<>:""/\\|?*]*";
        var pathMatch = Regex.Match(cmd, $@"-mod_folder ""?({pattern})""?");

        if (pathMatch.Success)
        {
            Common.ModDir = pathMatch.Groups[1].Value;
            Log.Debug($"Mod folder override: {Common.ModDir}");
        }
    }

    private class ModMetadata
    {
        public int LoadOrder;
        public string? OrigAsmPath;
        public string? LoadAsmPath;
        public bool Disabled;
    }
    
    private static void CheckModFolderOverride()
    {
        try
        {
            var cmd = USystemLibrary.GetCommandLine();
            const string pattern = """[a-zA-Z]:\\(?:[^<>:"/\\|?*]+\\)*[^<>:"/\\|?*]*""";
            var pathMatch = Regex.Match(cmd, $"""-mod_folder "?({pattern})"?""");

            if (pathMatch.Success)
            {
                Common.ModDir = pathMatch.Groups[1].Value;
                Log.Debug($"Mod folder override: {Common.ModDir}");
            }
            else
            {
                Log.Debug($"Mod folder: {Common.ModDir}");
            }
        }
        catch (Exception e)
        {
            Log.Error($"Resolve mod path failed: {e}");
            throw;
        }
    }

    public void LoadMods(bool reload, Dictionary<string, object>? reloadContexts)
    {
        try
        {
            LoadedMods.Clear();
            if (!Directory.Exists(Common.ModDir))
            {
                Log.Error($"Mod dir {Common.ModDir} not exists");
                return;
            }

            var allDirs = Directory.GetDirectories(Common.ModDir);
            var dirs = new List<string>();
            foreach (var dir in allDirs)
            {
                var modName = Path.GetFileName(dir);
                if (modName == "Common" || modName == "ReflectionOnly")
                    continue;

                dirs.Add(dir);
            }

            var meta = new Dictionary<string, ModMetadata>();
            foreach (var dir in dirs)
            {
                var disabledPath = Path.Combine(dir, "disabled.txt");
                var orderPath = Path.Combine(dir, "order.txt");
                var developPath = Path.Combine(dir, "develop.txt");
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

                var modMeta = new ModMetadata()
                {
                    LoadOrder = loadOrder ?? 0,
                    Disabled = File.Exists(disabledPath),
                };

                meta.Add(dir, modMeta);
            }

            dirs.Sort(Comparer<string>.Create((x, y) => meta[x].LoadOrder.CompareTo(meta[y].LoadOrder)));

            var copyHelper = new AssemblyCopyHelper();
            var renameHelper = new AssemblyRenameHelper();

            CloneDir = copyHelper.GetTempPath();
            copyHelper.SetReloadSuffix($"__{_reloadCounter++}");

            Log.Debug("======== Marking develop assemblies ========");

            foreach (var d in meta)
            {
                var dir = d.Key;
                var modMeta = d.Value;
                var modName = Path.GetFileName(dir);

                LoadingModName = modName;

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

                    if (UseReload)
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

                var modName = Path.GetFileName(dir);
                LoadingModName = modName;

                Log.Debug($"Processing {modName}");

                if (modMeta.Disabled)
                {
                    Log.Debug("Mod disabled");
                    continue;
                }

                var resolver = new DefaultAssemblyResolver();
                resolver.AddSearchDirectory(dir);
                resolver.AddSearchDirectory(Path.Combine(Common.ModDir, "Common"));
                resolver.AddSearchDirectory(Path.Combine(Common.ModDir, "ReflectionOnly"));
                if (CloneDir != null)
                {
                    resolver.AddSearchDirectory(Path.Combine(CloneDir, LoadingModName));
                    resolver.AddSearchDirectory(Path.Combine(CloneDir, "Common"));
                    resolver.AddSearchDirectory(Path.Combine(CloneDir, "ReflectionOnly"));
                }

                resolver.AddSearchDirectory(Common.LoaderDir);

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
                    if (UseReload)
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
            foreach (var dir in dirs)
            {
                var modName = Path.GetFileName(dir);

                LoadingModName = modName;
                var modMeta = meta[dir];

                if (modMeta.Disabled)
                    continue;

                if (modMeta.LoadAsmPath == null)
                {
                    Log.Debug($"No assembly to load for: {LoadingModName}");
                    continue;
                }

                try
                {
                    Log.Debug($"======== Loading {modMeta.LoadAsmPath} ========");
                    Assembly asm;

                    LoadResourceDlls(dir);
                    asm = Assembly.LoadFrom(modMeta.LoadAsmPath);

                    foreach (var type in asm.GetTypes())
                    {
                        if (csharpModType.IsAssignableFrom(type))
                        {
                            Log.Debug($"Found ICSharpMod: {type}");

                            if (Activator.CreateInstance(type) is ICSharpMod mod)
                            {
                                mod.Init();
                                LoadedMods.Add(mod);
                                Log.Debug($"Loaded mod {mod.Name} {mod.Version}");
                                
                                if (reload && mod is ICSharpModEx modEx)
                                {
                                    reloadContexts!.TryGetValue(modEx.Name, out var reloadContext);
                                    modEx.Reload(reloadContext);
                                    Log.Debug($"Reloaded mod {mod.Name} {mod.Version}");
                                }
                            }
                        }
                    }

                    LoadingModName = null;
                }
                catch (Exception ex)
                {
                    Log.Error($"Load {modMeta.LoadAsmPath} failed:");
                    Log.Error(ex);
                }
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex);
            throw;
        }
    }

    private void LoadResourceDlls(string dir)
    {
        try
        {
            Log.Debug($"Loading resources");
            string[] dllFiles = Directory.GetFiles(dir, "*.resources.dll", SearchOption.AllDirectories);
            foreach (var dll in dllFiles)
            {
                Assembly loadedAssembly = Assembly.LoadFrom(dll);
                Log.Debug($"Loaded: {loadedAssembly.FullName}");
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex);
        }
    }

    public Dictionary<string, object> GetReloadContexts()
    {
        var result = new Dictionary<string, object>();
        foreach (var mod in LoadedMods)
        {
            Log.Debug($"GetReloadContexts for {mod.Name}");
            if (mod is ICSharpModEx modEx)
            {
                var reloadContext = modEx.GetReloadContext();
                Log.Debug($"Received context {reloadContext}");
                if (reloadContext != null)
                    result.Add(modEx.Name, reloadContext);
            }
        }
        
        return result;
    }

    public void ReloadMods()
    {
        var reloadContexts = GetReloadContexts();
        
        Log.Debug("ReloadMods");
        InputManager.Clear();
        foreach (var mod in LoadedMods.AsEnumerable().Reverse())
        {
            try
            {
                mod.DeInit();
            }
            catch (Exception e)
            {
                Log.Error($"DeInit {mod.Name} failed:");
                Log.Error(e);
            }
        }

        LoadMods(true, reloadContexts);
    }

    public void StartLoop()
    {
        InputManager.RegisterBuiltinKeyBind(ModifierKeys.Control, Key.F5, ReloadMods);
        _loopThread = new Thread(Loop)
        {
            // IsBackground = true,
        };
        _loopThread.Start();
    }

    private void Loop()
    {
        while (true)
        {
            InputManager.Update();
            Thread.Sleep(10); // 10ms
        }
    }
}
