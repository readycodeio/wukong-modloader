using System;

namespace CSharpModBase;

public static class Log
{
    public static void Trace(string message)
        => ReadyM.Loader.Wukong.Bootstrap.LegacyLog.LogTrace(message);
    
    public static void Debug(string message)
        => ReadyM.Loader.Wukong.Bootstrap.LegacyLog.LogDebug(message);
    
    public static void Info(string message)
        => ReadyM.Loader.Wukong.Bootstrap.LegacyLog.LogInformation(message);

    public static void Warn(string message)
        => ReadyM.Loader.Wukong.Bootstrap.LegacyLog.LogWarning(message);

    public static void WarnIf(bool condition, string message)
    {
        if (condition)
            Warn(message);
    }

    public static void Error(string message)
        => ReadyM.Loader.Wukong.Bootstrap.LegacyLog.LogError(message);

    public static void Error(Exception ex)
        => ReadyM.Loader.Wukong.Bootstrap.LegacyLog.LogError(ex, "Unexpected error");
}
