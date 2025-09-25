using Microsoft.Extensions.Logging;

namespace ReadyM.Loader.Wukong.Bootstrap.Settings;

public class PathSettingsFactory(IpcHelper ipcHelper, ILogger logger)
{
    public PathSettings CreateSettings()
    {
        var baseDir = Directory.GetCurrentDirectory();
        if (!baseDir.EndsWith("Win64"))
            baseDir = Path.Combine(baseDir, "b1\\Binaries\\Win64");
        
        GetModFolderOverride(out var modDirOverride);
        var pathSettings = new PathSettings(baseDir, modDirOverride);

        if (pathSettings.ModDirOverride != null)
            logger.LogDebug("Mod folder override: {Path}", pathSettings.ModDir);
        else
            logger.LogDebug("Mod folder: {Path}", pathSettings.ModDir);
        
        return pathSettings;
    }
    
    private void GetModFolderOverride(out string? modDirOverride)
    {
        modDirOverride = null;
        
        var ipcHandshakeFile = ipcHelper.ReadIpcHandshakeFile();

        if (ipcHandshakeFile.TryGetValue("MOD_FOLDER", out var modFolder))
        {
            modDirOverride = modFolder;
        }
    }
}