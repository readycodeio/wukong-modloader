using System.Collections.Generic;
using System.Reflection;

namespace CSharpModBase;

/// Which folder each mod assembly was loaded from, in load order.
public static class LoadedMods
{
    private static readonly List<ModAssembly> Loaded = new();

    private static readonly object Gate = new object();

    /// Called by the loader as each mod assembly is loaded, so the order is the load order.
    public static void Add(Assembly assembly, string directory)
    {
        lock (Gate)
        {
            Loaded.Add(new ModAssembly(assembly, directory));
        }
    }

    public static IReadOnlyList<ModAssembly> All
    {
        get
        {
            lock (Gate)
            {
                return Loaded.ToArray();
            }
        }
    }

    public static void Clear()
    {
        lock (Gate)
        {
            Loaded.Clear();
        }
    }
}

/// One loaded mod assembly and the folder it came from.
public readonly struct ModAssembly
{
    public ModAssembly(Assembly assembly, string directory)
    {
        Assembly = assembly;
        Directory = directory;
    }

    public Assembly Assembly { get; }

    /// The real mod folder, never the reload-mode assembly clone.
    public string Directory { get; }
}
