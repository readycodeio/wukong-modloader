using IniParser;
using Microsoft.Extensions.Logging;
using ReadyM.Loader.Wukong.Bootstrap;

namespace ReadyM.Loader.Wukong.Managed;

public class ModLoaderSettingsFactory(PathSettings pathSettings, ILogger logger)
{
    public ModLoaderSettings CreateSettings()
    {
        logger.LogDebug("Setting up ModLoader");

        var baseDir = Directory.GetCurrentDirectory();
        if (!baseDir.EndsWith("Win64"))
            baseDir = Path.Combine(baseDir, "b1\\Binaries\\Win64");
        
        GetModeFromIniConfig(pathSettings.LoaderDir, out bool useDevelop, out var useReload);
        
        var settings = new ModLoaderSettings(useDevelop, useReload);

        return settings;
    }
    
    private void GetModeFromIniConfig(string loaderDir, out bool useDevelop, out bool useReload)
    {
        var developValue = "0";
        try
        {
            var iniPath = Path.Combine(loaderDir, "b1cs.ini");
            var fullIniPath = Path.GetFullPath(iniPath);
            var iniParser = new FileIniDataParser();
            var iniData = iniParser.ReadFile(fullIniPath);
            developValue = iniData["Settings"]?.GetKeyData("Develop")?.Value ?? "0";
        }
        catch (Exception ex)
        {
            logger.LogError(ex, "Error while parsing b1cs.ini");
        }
        
        useDevelop = developValue == "1" || developValue == "2";
        useReload = developValue == "2";

        logger.LogDebug("Develop Mode: {DevelopMode}", useDevelop);
        logger.LogDebug("Reload Mode: {ReloadMode}", useReload);
    }
}