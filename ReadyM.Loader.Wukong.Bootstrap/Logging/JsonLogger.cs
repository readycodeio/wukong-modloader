using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Logging.Console;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

internal class JsonLogger(JsonLoggerWorker worker, string? categoryName, ConsoleFormatter jsonConsoleFormatter) : ILogger
{
    public IDisposable BeginScope<TState>(TState state) where TState : notnull
        => null!;

    public bool IsEnabled(LogLevel logLevel)
        => true;
    
    [ThreadStatic]
    private static StringWriter? _stringWriter;

    public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
    {
        if (!IsEnabled(logLevel))
            return;

        var logEntry = new LogEntry<TState>(logLevel, categoryName ?? "", eventId, state, exception, formatter);
        
        _stringWriter ??= new StringWriter();
        jsonConsoleFormatter.Write(logEntry, null, _stringWriter);

        var jsonText = _stringWriter.ToString();
        _stringWriter.GetStringBuilder().Clear();
        
        worker.Enqueue(jsonText);
    }
}
