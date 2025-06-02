using EmbedCSharpLoader.Managed;

internal class Program
{
    public static void Main(string[] args)
    {
        CSharpModManager manager = new();
        manager.LoadMods(false, null);
        manager.StartLoop();
    }
}
