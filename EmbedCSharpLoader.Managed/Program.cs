using CSharpModBase;
using EmbedCSharpLoader.Managed;

internal class Program
{
    public static void Main(string[] args)
    {
        Log.Debug("Managed loader");
     
        CSharpModManager manager = new();
        manager.LoadMods(false, null);
        manager.StartLoop();
    }
}
