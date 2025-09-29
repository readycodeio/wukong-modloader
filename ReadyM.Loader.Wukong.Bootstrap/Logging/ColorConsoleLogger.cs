using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Logging.Console;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

internal class ColorConsoleLogger(string? categoryName, bool autoFlush, ConsoleFormatter consoleFormatter) : ILogger
{
    private static readonly object LockObj = new();

    public IDisposable BeginScope<TState>(TState state)
        where TState : notnull
        => null!;

    public bool IsEnabled(LogLevel logLevel)
        => true;

    private readonly StringWriter _stringWriter = new();

    public void Log<TState>(
        LogLevel logLevel,
        EventId eventId,
        TState state,
        Exception? exception,
        Func<TState, Exception?, string> formatter)
    {
        if (!IsEnabled(logLevel))
            return;

        var color = logLevel switch
        {
            LogLevel.Trace => ConsoleColor.DarkGray,
            LogLevel.Debug => ConsoleColor.Gray,
            LogLevel.Information => ConsoleColor.White,
            LogLevel.Warning => ConsoleColor.Yellow,
            LogLevel.Error => ConsoleColor.Red,
            LogLevel.Critical => ConsoleColor.Magenta,
            LogLevel.None => ConsoleColor.Cyan,
            _ => throw new ArgumentOutOfRangeException(nameof(logLevel), logLevel, null)
        };

        lock (LockObj)
        {
            var originalColor = Console.ForegroundColor;
            try
            {
                Console.ForegroundColor = color;
                var logEntry = new LogEntry<TState>(logLevel, categoryName ?? "", eventId, state, exception, formatter);

                consoleFormatter.Write(logEntry, null, _stringWriter);

                var line = _stringWriter.ToString();
                _stringWriter.GetStringBuilder().Clear();

                Console.WriteLine(line);
            }
            finally
            {
                Console.ForegroundColor = originalColor;
            }

            if (autoFlush)
            {
                Console.Out.Flush();
            }
        }
    }
}