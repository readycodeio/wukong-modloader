using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Console;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

internal class TextWriterLoggerProvider(bool autoFlush, TextWriter writer, ConsoleFormatter consoleFormatter) : ILoggerProvider
{
    public ILogger CreateLogger(string? categoryName)
        => CreateLoggerTyped(categoryName);
    
    public TextWriterLogger CreateLoggerTyped(string? categoryName)
        => new(categoryName, autoFlush, writer, consoleFormatter);

    public void Dispose()
    {
        // empty
    }
}
