using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Console;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

internal class ColorConsoleLoggerProvider(bool autoFlush, ConsoleFormatter consoleFormatter) : ILoggerProvider
{
    public ILogger CreateLogger(string? categoryName)
        => CreateLoggerTyped(categoryName);
    
    public ColorConsoleLogger CreateLoggerTyped(string? categoryName)
        => new(categoryName, autoFlush, consoleFormatter);

    public void Dispose()
    {
        // empty
    }
}
