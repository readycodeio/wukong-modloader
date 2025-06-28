using System.Collections.Concurrent;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

internal class JsonLoggerWorker : IDisposable
{
    private readonly string _jsonLogDirPath;
    private string _currentLogFile;
    private readonly ConcurrentQueue<string> _logQueue = new();
    private readonly AutoResetEvent _logSignal = new(false);
    private readonly Thread _logThread;
    private const long MaxLogFileSize = 5 * 1024 * 1024; // 5 MB
    private volatile bool _isRunning = true;

    public JsonLoggerWorker(string jsonLogDirPath)
    {
        _jsonLogDirPath = jsonLogDirPath;
        Directory.CreateDirectory(jsonLogDirPath);
        _currentLogFile = GetNewLogFilePath();

        _logThread = new Thread(ProcessLogQueue) { IsBackground = true };
        _logThread.Start();
    }

    public void Dispose()
    {
        _isRunning = false;
        _logSignal.Set();
        _logThread.Join();
    }
    
    private void ProcessLogQueue()
    {
        while (_isRunning || !_logQueue.IsEmpty)
        {
            _logSignal.WaitOne();
            WriteLogsToFile();
        }
    }

    private void WriteLogsToFile()
    {
        while (_logQueue.TryDequeue(out var logEntry))
        {
            RotateLogFileIfNeeded();

            using var fileStream = new FileStream(_currentLogFile, FileMode.Append, FileAccess.Write, FileShare.Read);
            using var writer = new StreamWriter(fileStream);
            writer.AutoFlush = true;
            writer.WriteLine(logEntry);
        }
    }

    private void RotateLogFileIfNeeded()
    {
        FileInfo fileInfo = new(_currentLogFile);
        if (fileInfo is { Exists: true, Length: >= MaxLogFileSize })
        {
            _currentLogFile = GetNewLogFilePath();
        }
    }

    private string GetNewLogFilePath()
    {
        return Path.Combine(_jsonLogDirPath, $"log_{DateTime.UtcNow:yyyyMMdd_HHmmss}.json");
    }

    public void Enqueue(string jsonText)
    {
        _logQueue.Enqueue(jsonText);
        _logSignal.Set();
    }
}