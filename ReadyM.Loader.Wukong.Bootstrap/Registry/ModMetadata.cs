 namespace ReadyM.Loader.Wukong.Bootstrap.Registry;

public class ModMetadata
{
    public string? ModName;
    public string? ModDir;
    public int LoadOrder;
    public bool Disabled;
    public List<string> AllAsmPaths = [];
}