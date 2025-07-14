using System.Diagnostics.CodeAnalysis;
using Microsoft.Extensions.Logging;

namespace ReadyM.Loader.Wukong.Bootstrap;

[SuppressMessage("Usage", "CA2254:Template should be a static expression")]
public static class LegacyLog
{
    private static ILogger _logger = null!;

    public static void SetLogger(ILogger logger)
    {
        _logger = logger;
    }
    
    public static void LogTrace(string message)
        => _logger.LogTrace(message);
    
    public static void LogDebug(string message)
        => _logger.LogDebug(message);
    
    public static void LogInformation(string message)
        => _logger.LogInformation(message);

    public static void LogWarning(string message)
        => _logger.LogWarning(message);

    public static void LogError(string message)
        => _logger.LogError(message);

    public static void LogError(Exception ex, string message)
        => _logger.LogError(ex, message);
}
