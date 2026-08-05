using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Logging.Console;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

internal class TextWriterLogger(string? categoryName, Func<bool> shouldFlush, TextWriter writer, ConsoleFormatter consoleFormatter) : ILogger
{
    [ThreadStatic]
    private static StringWriter? _stringWriter;

    public IDisposable? BeginScope<TState>(TState state)
        where TState : notnull
        => null;

    public bool IsEnabled(LogLevel logLevel)
        => true;

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

        _stringWriter ??= new StringWriter();
        consoleFormatter.Write(logEntry, null, _stringWriter);

        var line = _stringWriter.ToString();
        _stringWriter.GetStringBuilder().Clear();

        writer.WriteLine(line);

        if (shouldFlush())
        {
            writer.Flush();
        }
    }
}