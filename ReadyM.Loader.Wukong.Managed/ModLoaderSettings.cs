namespace ReadyM.Loader.Wukong.Managed;

public class ModLoaderSettings(bool useDevelop, bool useReload)
{
    public readonly bool UseDevelop = useDevelop;
    public readonly bool UseReload = useReload;
}