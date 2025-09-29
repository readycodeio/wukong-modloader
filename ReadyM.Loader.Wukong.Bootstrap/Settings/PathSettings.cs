namespace ReadyM.Loader.Wukong.Bootstrap.Settings;

public class PathSettings(string baseDir, string? modDirOverride)
{
    public const string ModDirSuffix = "CSharpLoader\\Mods";

    public readonly string BaseDir = baseDir;
    public readonly string? ModDirOverride = modDirOverride;

    public string LoaderDir => GetLoaderDir(BaseDir);
    public string DataDir => GetDataDir(BaseDir);
    public string ModDir => ModDirOverride ?? GetDefaultModDir(BaseDir);

    public static string GetLoaderDir(string baseDir)
        => Path.Combine(baseDir, "CSharpLoader");
    
    public static string GetDataDir(string baseDir)
        => Path.Combine(baseDir, "CSharpLoader\\Data");
    
    public static string GetDefaultModDir(string baseDir)
        => Path.Combine(baseDir, ModDirSuffix);    
}