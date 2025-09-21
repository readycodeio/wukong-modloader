using Microsoft.Extensions.Logging;
using Microsoft.Win32.SafeHandles;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

public class LoggerFactoryProvider : IDisposable
{
    private readonly CustomJsonFormatter _jsonFormatter;
    private readonly CustomTextFormatter _textFormatter = new();

    private readonly JsonLoggerWorker _worker;
    private readonly List<ILoggerFactory> _loggerFactories = [];
    private readonly TextWriter _threadSafeLogFileWriter;

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
        var factory = LoggerFactory.Create(builder =>
        {
            builder.AddProvider(new JsonLoggerProvider(_worker, _jsonFormatter));
            builder.AddProvider(new ColorConsoleLoggerProvider(autoFlush, _textFormatter));
            builder.AddProvider(new TextWriterLoggerProvider(autoFlush, _threadSafeLogFileWriter, _textFormatter));
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
}
