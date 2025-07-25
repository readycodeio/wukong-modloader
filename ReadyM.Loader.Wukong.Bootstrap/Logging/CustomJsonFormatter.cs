using System.Diagnostics;
using System.Reflection;
using System.Text.Json;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Logging.Console;
using ReadyM.Loader.Wukong.Bootstrap.Logging.Unreal;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

internal class CustomJsonFormatter(Guid sessionId) : ConsoleFormatter("custom-json")
{
    private readonly JsonSerializerOptions _options = new(JsonSerializerOptions.Default)
    {
        WriteIndented = false,
        Converters =
        {
            new FTextConverter()
        }
    };

    private static readonly Dictionary<string, object> EmptyProperties = new();

    public override void Write<TState>(
        in LogEntry<TState> logEntry,
        IExternalScopeProvider? scopeProvider,
        TextWriter textWriter)
    {
        string? messageTemplate = null;

        var props = EmptyProperties;
        if (logEntry.State is IEnumerable<KeyValuePair<string, object>> state)
        {
            props = new Dictionary<string, object>();

            foreach (var kv in state)
            {
                if (kv.Key == "{OriginalFormat}")
                {
                    messageTemplate = kv.Value as string;
                    continue;
                }

                props[kv.Key] = kv.Value;
            }
        }

        if (logEntry.Exception != null)
        {
            messageTemplate = $"Exception: {{Message}} | Thread: {{Thread}} | Stack trace: {{Trace}} | Context: {messageTemplate}";
            props["Message"] = logEntry.Exception.Message;
            props["Thread"] = Thread.CurrentThread.ManagedThreadId;
            props["Trace"] = logEntry.Exception.StackTrace;
        }

        if (logEntry.LogLevel is LogLevel.Error or LogLevel.Critical)
        {
            var skip = 7;
            MethodBase caller;
            do
            {
                var frame = new StackFrame(skip);
                var method = frame.GetMethod();
                var methodName = method?.Name ?? "";
                if (methodName.EndsWith("LoggingExtensions") || methodName.EndsWith("Logging"))
                {
                    skip++;
                    continue;
                }

                caller = method!;
                break;
            } while (true);

            props[LoggerConstants.ThreadIdPropertyName] = Environment.CurrentManagedThreadId;
            props[LoggerConstants.LocationPropertyName] = $"{caller.DeclaringType?.FullName}.{caller.Name}";

            messageTemplate += $" [thread {LoggerConstants.ThreadIdPropertyName} at {LoggerConstants.LocationPropertyName}]";
        }

        var record = new
        {
            TimeGenerated = DateTime.UtcNow.ToString("o"),
            Level = logEntry.LogLevel.ToString(),
            MessageTemplate = messageTemplate ?? "",
            Properties = props,
            Session = sessionId,
            // Category = logEntry.Category,
        };

        var jsonText = JsonSerializer.Serialize(record, _options);
        textWriter.Write(jsonText);
    }
}