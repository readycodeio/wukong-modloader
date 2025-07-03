namespace ReadyM.Loader.Wukong.Bootstrap;

public static class ModLoaderSettings
{
    public static bool UseDevelop { get; set; }
    public static bool UseReload { get; set; }

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

    public static string ModDirSuffix { get; set; } = "CSharpLoader\\Mods";

    public static string LoaderDir { get; set; } = Path.Combine(BaseDir, "CSharpLoader");
    
    public static string? ModDirOverride { get; set; }
    public static string ModDir => ModDirOverride ?? Path.Combine(BaseDir, ModDirSuffix);
    public static string DataDir => Path.Combine(BaseDir, "CSharpLoader\\Data");
}
