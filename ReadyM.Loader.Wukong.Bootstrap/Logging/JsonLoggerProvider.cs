using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Console;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

internal class JsonLoggerProvider(JsonLoggerWorker worker, ConsoleFormatter formatter) : ILoggerProvider
{
    public ILogger CreateLogger(string? categoryName)
        => new JsonLogger(worker, categoryName, formatter);
    
    public void Dispose()
    {
        // empty
    }
}
