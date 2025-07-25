namespace ReadyM.Loader.Wukong.Bootstrap;

public static class Settings
{
    private static bool? _useDevelop = null!;
    private static bool? _useReload = null!;

    public static bool UseDevelop
    {
        get => _useDevelop!.Value;
        set => _useDevelop = value;
    }

    public static bool UseReload
    {
        get => _useReload!.Value;
        set => _useReload = value;
    }

    public static string? LoadingModName { get; set; }
    public static string? CloneDir { get; set; }

    private static string? _baseDir;
    
    public static string BaseDir
    {
        get
        {
            if (_baseDir == null)
            {
                _baseDir = Directory.GetCurrentDirectory();
                if (!_baseDir.EndsWith("Win64"))
                    _baseDir = Path.Combine(_baseDir, "b1\\Binaries\\Win64");
            }
            return _baseDir;
        }
    }

    public static string ModDirSuffix { get; } = "CSharpLoader\\Mods";

    public static string LoaderDir { get; } = Path.Combine(BaseDir, "CSharpLoader");
    
    public static string? ModDirOverride { get; set; }
    public static string ModDir => ModDirOverride ?? Path.Combine(BaseDir, ModDirSuffix);
    public static string DataDir => Path.Combine(BaseDir, "CSharpLoader\\Data");
}
