using System.Text.Json.Serialization;
using Microsoft.Extensions.Logging;
using Microsoft.Win32.SafeHandles;
using ReadyM.Loader.Wukong.Bootstrap.Settings;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

public class LoggerFactoryProvider : IDisposable
{
    private readonly CustomJsonFormatter _jsonFormatter;
    private readonly CustomTextFormatter _textFormatter = new();

    private readonly JsonLoggerWorker _worker;
    private readonly List<ILoggerFactory> _loggerFactories = [];
    private readonly TextWriter _threadSafeLogFileWriter;

    private volatile bool _forceFlush;

    /// <summary>
    /// While set, every log line is flushed immediately regardless of the per-factory autoFlush setting.
    /// </summary>
    /// <remarks>
    /// Mod loggers are created with autoFlush off because a flush per line is too expensive during gameplay,
    /// and the log loop thread only catches up every 100ms. That is fine until a mod takes the process down
    /// inside Init, at which point the last 100ms of its log, the part naming what it was doing, dies in the
    /// buffer. The mod loader turns this on for the load and init window and back off afterwards.
    /// </remarks>
    public bool ForceFlush
    {
        get => _forceFlush;
        set => _forceFlush = value;
    }

    public LoggerFactoryProvider(Guid guid, SafeFileHandle logFileHandle, PathSettings pathSettings)
    {
        var logFileStream = new FileStream(logFileHandle, FileAccess.Write);
        var logFileWriter = new StreamWriter(logFileStream);
        _threadSafeLogFileWriter = TextWriter.Synchronized(logFileWriter); 
     
        var jsonLoggerPath = $"{pathSettings.BaseDir}\\wukong-mp-logs";
        _worker = new JsonLoggerWorker(jsonLoggerPath);
        _jsonFormatter = new CustomJsonFormatter(guid);
    }

    public ILoggerFactory CreateLoggerFactory(bool debugMode, bool autoFlush)
    {
        // Evaluated per log line, so flipping ForceFlush affects loggers that already exist.
        bool ShouldFlush() => autoFlush || _forceFlush;

        var factory = LoggerFactory.Create(builder =>
        {
            builder.AddProvider(new JsonLoggerProvider(_worker, _jsonFormatter));
            builder.AddProvider(new ColorConsoleLoggerProvider(ShouldFlush, _textFormatter));
            builder.AddProvider(new TextWriterLoggerProvider(ShouldFlush, _threadSafeLogFileWriter, _textFormatter));
            builder.SetMinimumLevel(debugMode ? LogLevel.Debug : LogLevel.Information);
        });
        _loggerFactories.Add(factory);
        return factory;
    }

    public void Dispose()
    {
        foreach (var factory in _loggerFactories)
        {
            factory.Dispose();
        }
        
        _worker.Dispose();
    }

    public void Flush()
    {
        Console.Out.Flush();
        _threadSafeLogFileWriter.Flush();
    }
    
    public void RegisterConverter(JsonConverter converter)
    {
        _jsonFormatter.RegisterConverter(converter);
    }
}
