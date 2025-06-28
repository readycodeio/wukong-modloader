using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Logging.Console;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

internal class TextWriterLogger(string? categoryName, bool autoFlush, TextWriter writer, ConsoleFormatter consoleFormatter) : ILogger
{
    public IDisposable BeginScope<TState>(TState state)
        where TState : notnull
        => null!;

    public bool IsEnabled(LogLevel logLevel)
        => true;
    
    private readonly StringWriter _stringWriter = new StringWriter();

    public void Log<TState>(
        LogLevel logLevel,
        EventId eventId,
        TState state,
        Exception? exception,
        Func<TState, Exception?, string> formatter)
    {
        if (!IsEnabled(logLevel))
            return;

        var logEntry = new LogEntry<TState>(logLevel, categoryName ?? "", eventId, state, exception, formatter);
        
        consoleFormatter.Write(logEntry, null, _stringWriter);

        var line = _stringWriter.ToString();
        _stringWriter.GetStringBuilder().Clear();
        
        writer.WriteLine(line);
        
        if (autoFlush)
        {
            writer.Flush();
        }
    }
}
