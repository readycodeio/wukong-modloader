namespace CSharpModBase;

public interface ICSharpMod
{
    /// <summary>
    /// The mod name
    /// </summary>
    string Name { get; }

    /// <summary>
    /// Called when mod is loaded, immediately after the USharp mono embedded runtime is initialized, or after
    /// mod reloading.
    /// </summary>
    void Init();
    
    /// <summary>
    /// Called when mod is unloaded, before the game exits, or before mod reloading.
    /// </summary>
    void DeInit();
}