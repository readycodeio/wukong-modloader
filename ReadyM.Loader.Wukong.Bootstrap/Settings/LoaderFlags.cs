using System.Runtime.InteropServices;

namespace ReadyM.Loader.Wukong.Bootstrap.Settings;

/// <summary>
/// Flags read from b1cs.ini. Uses the same Win32 call the native loader uses for its own flags, so the parsing
/// semantics are identical and there is no second ini parser to disagree with the first.
/// </summary>
public class LoaderFlags
{
    private const string Section = "Settings";

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetPrivateProfileIntW(string section, string key, int defaultValue, string filePath);

    public LoaderFlags(PathSettings pathSettings)
    {
        var iniPath = Path.Combine(pathSettings.LoaderDir, "b1cs.ini");
        PatchGameAssemblies = GetPrivateProfileIntW(Section, "PatchGameAssemblies", 1, iniPath) != 0;
    }

    /// <summary>
    /// When false, the game's own bundled assemblies are left byte for byte as the game shipped them: they are
    /// still read and rewritten in memory, so the same code paths run, but the rewritten copies are never
    /// installed and Mono goes on loading the originals. Mod assemblies are still patched as normal.
    /// </summary>
    /// <remarks>
    /// Diagnostic switch, default on. It exists to test one specific hypothesis: two players crash inside the
    /// game's obfuscated code with 15 of 17 registers identical across machines, reached from Mono while
    /// class-initialising a type whose static init pulls in the assemblies we rewrite. Turning this off means
    /// Mono never loads a rewritten game assembly, so if Init then gets further, the rewriting is implicated.
    /// The mod will not actually function in that state, because none of its game-side hooks are installed.
    /// </remarks>
    public bool PatchGameAssemblies { get; }
}
