using Microsoft.Extensions.Logging;

namespace CSharpModBase;

public interface ICSharpModExV2 : ICSharpModEx
{
    bool IsDebug { get; }
    void SetLoggerFactory(ILoggerFactory loggerFactory);

    /// <summary>
    /// The mod's own folder under <c>Mods</c>, where its files live. Pushed in by the loader rather than
    /// derived from the assembly, whose location is a clone of the original in reload mode.
    /// </summary>
    void SetModDirectory(string directory);

    void LateInit();
}
