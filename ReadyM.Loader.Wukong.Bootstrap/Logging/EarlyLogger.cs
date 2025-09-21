using Microsoft.Extensions.Logging;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

public class EarlyLogger : ILogger
{
    private abstract class EntryBase
    {
        public abstract void Append(ILogger logger);
    }

    private class Entry<TState>(
        LogLevel logLevel,
        EventId eventId,
        TState state,
        Exception? exception,
        Func<TState, Exception?, string> formatter) : EntryBase
    {
        public override void Append(ILogger logger)
            => logger.Log(logLevel, eventId, state, exception, formatter);
    }

    private ILogger? _attachedLogger;
    private readonly List<EntryBase> _entries = [];
    
    public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
    {
        if (_attachedLogger != null)
        {
            _attachedLogger.Log(logLevel, eventId, state, exception, formatter);
            return;
        }

        var entry = new Entry<TState>(logLevel, eventId, state, exception, formatter);
        _entries.Add(entry);
    }

    public bool IsEnabled(LogLevel logLevel)
        => true;

    public IDisposable? BeginScope<TState>(TState state)
        where TState : notnull
        => null;

    public void Attach(ILogger logger)
    {
        foreach (var entry in _entries)
            entry.Append(logger);
        _entries.Clear();
        _attachedLogger = logger;
    }
}