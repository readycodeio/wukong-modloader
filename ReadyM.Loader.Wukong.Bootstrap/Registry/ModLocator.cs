using Microsoft.Extensions.Logging;
using ReadyM.Loader.Wukong.Bootstrap;
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
            if (modName == "Common" || modName == "ReflectionOnly" || modName == "Overrides")
                continue;

            dirs.Add(dir);
        }

        var meta = new Dictionary<string, ModMetadata>();
        foreach (var dir in dirs)
        {
            var disabledPath = Path.Combine(dir, "disabled.txt");
            var orderPath = Path.Combine(dir, "order.txt");
            int? loadOrder = null;
            if (File.Exists(orderPath))
            {
                var orderStr = File.ReadAllText(orderPath).Trim();
                if (int.TryParse(orderStr, out var parsedOrder))
                {
                    loadOrder = parsedOrder;
                }
            }

            if (loadOrder == null)
                logger.LogDebug("Default order: {Path}", dir);
            else
                logger.LogDebug("Order: {Order} {Path}", loadOrder, dir);


            var modName = Path.GetFileName(dir);
            var modMeta = new ModMetadata
            {
                ModName = modName!,
                ModDir = dir,
                Disabled = File.Exists(disabledPath),
                LoadOrder = loadOrder ?? 0,
            };

            var mainAsmPath = Path.Combine(dir, $"{modName}.dll");
            if (File.Exists(mainAsmPath))
                modMeta.MainAsmPath = mainAsmPath;
            
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
}