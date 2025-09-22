namespace ReadyM.Loader.Wukong.Bootstrap.Registry;

public class ModRegistry
{
    private readonly Dictionary<string, ModMetadata> _metaByDir = [];
    private readonly List<string> _modNames = [];

    public void Clear()
    {
        _metaByDir.Clear();
        _modNames.Clear();
    }
    
    public void AddMod(ModMetadata meta)
    {
        if (_metaByDir.ContainsKey(meta.ModDir))
            throw new Exception($"Mod already registered: {meta.ModDir}");

        _metaByDir.Add(meta.ModDir, meta);
        _modNames.Add(meta.ModDir);
    }
    
    public IReadOnlyList<string> ModDirs
        => _modNames.AsReadOnly();
    
    public IReadOnlyDictionary<string, ModMetadata> MetaByDir
        => _metaByDir;
}