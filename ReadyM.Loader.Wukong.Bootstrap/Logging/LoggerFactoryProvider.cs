using Microsoft.Extensions.Logging;
using Microsoft.Win32.SafeHandles;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

public class LoggerFactoryProvider : IDisposable
{
    private static readonly string JsonLoggerPath = $"{Settings.BaseDir}\\wukong-mp-logs";

    private readonly CustomJsonFormatter _jsonFormatter;
    private readonly CustomTextFormatter _textFormatter = new();

    private readonly JsonLoggerWorker _worker = new(JsonLoggerPath);
    private readonly List<ILoggerFactory> _loggerFactories = new();
    private readonly TextWriter _threadSafeLogFileWriter;

    public LoggerFactoryProvider(Guid guid, SafeFileHandle logFileHandle)
    {
        var logFileStream = new FileStream(logFileHandle, FileAccess.Write);
        var logFileWriter = new StreamWriter(logFileStream);
        _threadSafeLogFileWriter = TextWriter.Synchronized(logFileWriter); 
        
        _jsonFormatter = new CustomJsonFormatter(guid);
    }

    public ILoggerFactory CreateLoggerFactory(bool debugMode, bool autoFlush)
    {
        var factory = LoggerFactory.Create(builder =>
        {
            builder.AddProvider(new JsonLoggerProvider(_worker, _jsonFormatter));
            builder.AddProvider(new ColorConsoleLoggerProvider(autoFlush, _textFormatter));
            builder.AddProvider(new TextWriterLoggerProvider(autoFlush, _threadSafeLogFileWriter, _textFormatter));
            builder.SetMinimumLevel(debugMode ? LogLevel.Debug : LogLevel.Error);
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
}
