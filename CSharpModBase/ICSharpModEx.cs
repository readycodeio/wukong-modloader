namespace CSharpModBase;

public interface ICSharpModEx : ICSharpMod
{
    object? GetReloadContext();
    void Reload(object? context);
}