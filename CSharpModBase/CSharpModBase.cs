using System.IO;

namespace CSharpModBase;

public static class Common
{
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

    public static string LoaderDir = Path.Combine(BaseDir, "CSharpLoader");
    public static string ModDir => Path.Combine(BaseDir, ModDirSuffix);
    public static string DataDir => Path.Combine(BaseDir, "CSharpLoader\\Data");
}


public interface ICSharpMod
{
    /// <summary>
    /// mod name
    /// </summary>
    string Name { get; }

    /// <summary>
    /// mod version
    /// </summary>
    string Version { get; }

    /// <summary>
    /// when mod loaded, will call OnInit
    /// </summary>
    void Init();
    /// <summary>
    /// when manamger reload mods, will call OnDeInit,
    ///
    /// </summary>
    void DeInit();
}

public interface ICSharpModEx : ICSharpMod
{
    object? GetReloadContext();
    void Reload(object? context);
}
