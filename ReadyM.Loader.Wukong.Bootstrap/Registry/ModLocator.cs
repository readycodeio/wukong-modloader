using System.Text.Json;
using Microsoft.Extensions.Logging;
using ReadyM.Loader.Wukong.Bootstrap.Settings;

namespace ReadyM.Loader.Wukong.Bootstrap.Registry;

public class ModLocator(PathSettings pathSettings, ILogger logger)
{
    public ModRegistry LocateMods()
    {
        if (!Directory.Exists(pathSettings.ModDir))
        {
            logger.LogError("Mod dir {Path} not exists", pathSettings.ModDir);
            return new ModRegistry();
        }

        var allDirs = Directory.GetDirectories(pathSettings.ModDir);
        var dirs = new List<string>();
        foreach (var dir in allDirs)
        {
            var modName = Path.GetFileName(dir);
            if (modName is "ReflectionOnly")
                continue;

            dirs.Add(dir);
        }

        var manifests = new Dictionary<string, ModManifest>();
        var options = new JsonSerializerOptions
        {
            PropertyNameCaseInsensitive = true,
            ReadCommentHandling = JsonCommentHandling.Skip,
            AllowTrailingCommas = true
        };

        foreach (var dir in dirs)
        {
            var disabledPath = Path.Combine(dir, "disabled.txt");
            var manifestPath = Path.Combine(dir, ModManifest.FileName);

            if (File.Exists(disabledPath))
            {
                logger.LogWarning("Mod {Path} is disabled", dir);
            }

            if (!File.Exists(manifestPath))
            {
                logger.LogError("Mod {Path} doesn't contain a manifest file ({Manifest}), skipping", dir, ModManifest.FileName);
                continue;
            }

            var manifest = JsonSerializer.Deserialize<ModManifest>(File.ReadAllText(manifestPath), options);

            if (manifest == null)
            {
                logger.LogError("Mod {Path} manifest file is invalid, skipping", dir);
                continue;
            }

            manifests.Add(dir, manifest);
        }

        if (!TryCalculateLoadOrders([.. manifests.Values], out var loadOrders))
        {
            logger.LogError("Failed to calculate mod load orders due to dependency issues. See previous errors for details.");
            return new ModRegistry();
        }

        var sortedManifests = manifests.Values.OrderBy(m => loadOrders[m.UniqueId]);
        logger.LogInformation("Mod load order:");
        foreach (var manifest in sortedManifests)
        {
            logger.LogInformation("- {ModName} (ID: {ModId}, Version: {Version})", manifest.Name, manifest.UniqueId, manifest.Version);
        }

        var meta = new Dictionary<string, ModMetadata>();
        foreach (var dir in dirs)
        {
            var disabledPath = Path.Combine(dir, "disabled.txt");
            var manifest = manifests[dir];
            var modMeta = new ModMetadata
            {
                ModName = manifest.Name,
                ModDir = dir,
                Disabled = File.Exists(disabledPath),
                LoadOrder = loadOrders[manifest.UniqueId]
            };
            
            foreach (var f in Directory.GetFiles(dir, "*.dll", SearchOption.AllDirectories))
            {
                if (f.EndsWith(".32.dll") || f.EndsWith(".64.dll") ||
                    f.EndsWith(".x86.dll") || f.EndsWith(".x64.dll") ||
                    f.EndsWith("-32.dll") || f.EndsWith("-64.dll") ||
                    f.EndsWith("-x86.dll") || f.EndsWith("-x64.dll"))
                    continue;

                modMeta.AllAsmPaths.Add(f);
            }
            
            meta.Add(dir, modMeta);
        }

        dirs.Sort(Comparer<string>.Create((x, y) => meta[x].LoadOrder.CompareTo(meta[y].LoadOrder)));

        var result = new ModRegistry();

        foreach (var dir in dirs)
        {
            result.AddMod(meta[dir]);
        }

        return result;
    }

    private bool TryCalculateLoadOrders(List<ModManifest> manifests, out Dictionary<string, int> order)
    {
        order = new Dictionary<string, int>();
        var manifestLookup = manifests.ToDictionary(m => m.UniqueId, m => m);
        var visited = new Dictionary<string, bool>(); // ID -> IsCurrentlyVisiting (for cycle detection)
        var sortedList = new List<string>();

        foreach (var mod in manifests)
        {
            if (!Visit(mod.UniqueId, visited, sortedList, manifestLookup))
            {
                order = new Dictionary<string, int>();
                return false;
            }
        }

        // Map the sorted list to the dictionary with indices
        for (int i = 0; i < sortedList.Count; i++)
        {
            order[sortedList[i]] = i;
        }

        return true;
    }

    private bool Visit(
        string id,
        Dictionary<string, bool> visited,
        List<string> sortedList,
        Dictionary<string, ModManifest> lookup)
    {
        // If already fully processed, skip
        if (sortedList.Contains(id)) return true;

        // If currently in the stack, we found a cycle
        if (visited.TryGetValue(id, out var isVisiting) && isVisiting)
        {
            logger.LogError("Circular dependency detected involving mod: {ModId}", id);
            return false;
        }

        // Mark as visiting
        visited[id] = true;

        if (!lookup.TryGetValue(id, out var manifest))
        {
            logger.LogError("Dependency not met: Mod '{ModId}' is missing from the load list.", id);
            return false;
        }

        foreach (var dependency in manifest.Dependencies)
        {
            // 1. Check if dependency exists
            if (!lookup.TryGetValue(dependency.UniqueId, out var depManifest))
            {
                logger.LogError("Mod '{ModId}' depends on '{DepId}', but it was not found.", id, dependency.UniqueId);
                return false;
            }

            // 2. Simple Version Check (Optional: Replace with a SemVer parser if needed)
            if (string.Compare(depManifest.Version, dependency.MinimumVersion, StringComparison.OrdinalIgnoreCase) < 0)
            {
                logger.LogError("Mod '{ModId}' requires '{DepId}' version {MinVer} or higher, but found {ActualVer}.",
                    id, dependency.UniqueId, dependency.MinimumVersion, depManifest.Version);
                return false;
            }

            // 3. Recurse
            if (!Visit(dependency.UniqueId, visited, sortedList, lookup))
                return false;
        }

        // Mark as finished and add to list
        visited[id] = false;
        sortedList.Add(id);

        return true;
    }
}