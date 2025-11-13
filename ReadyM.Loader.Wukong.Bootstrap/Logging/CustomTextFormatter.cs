using System.Diagnostics;
using System.Reflection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Logging.Console;

#if DEBUG
using System.Diagnostics;
using System.Reflection;
#endif

// ReSharper disable PossibleMultipleEnumeration

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

internal class CustomTextFormatter() : ConsoleFormatter("custom-text")
{
    private const string OriginalFormatField = "{OriginalFormat}";
    private const string LocationPropertyField = $"{{{LoggerConstants.LocationPropertyName}}}";

    private static string DateTimeString => DateTime.Now.ToString("MM-dd HH:mm:ss.fff");

    public override void Write<TState>(
        in LogEntry<TState> logEntry,
        IExternalScopeProvider? scopeProvider,
        TextWriter textWriter)
    {
        var timestamp = DateTimeOffset.Now.ToString(DateTimeString);
        var letter = logEntry.LogLevel switch
        {
            LogLevel.Trace => 'T',
            LogLevel.Debug => 'D',
            LogLevel.Information => 'I',
            LogLevel.Warning => 'W',
            LogLevel.Error => 'E',
            LogLevel.Critical => 'C',
            LogLevel.None => 'N',
            _ => throw new ArgumentOutOfRangeException(nameof(logEntry.LogLevel), logEntry.LogLevel, null)
        };

        var category = logEntry.Category;
        var interpolatedMessage = "<INTERPOLATION ERROR>";

        if (logEntry.State is IEnumerable<KeyValuePair<string, object?>> props)
        {
            foreach (var kv in props)
            {
                if (kv.Key == OriginalFormatField)
                {
                    interpolatedMessage = (string)kv.Value!;
                    break;
                }
            }

            foreach (var kv in props)
            {
                if (kv.Key == LocationPropertyField)
                {
                    interpolatedMessage = interpolatedMessage.Replace($"{{{kv.Key}}}", "<REDACTED>");
                    continue;
                }

                interpolatedMessage = interpolatedMessage.Replace($"{{{kv.Key}}}", kv.Value?.ToString() ?? "null");
            }
        }

        textWriter.Write(timestamp);
        textWriter.Write(" [");
        textWriter.Write(letter);
        textWriter.Write("]");

        if (!string.IsNullOrEmpty(category))
        {
            textWriter.Write(" [");
            textWriter.Write(category);
            textWriter.Write("]");
        }

        if (!string.IsNullOrEmpty(interpolatedMessage))
        {
            textWriter.Write(" ");
            textWriter.Write(interpolatedMessage);
        }

#if DEBUG
        if (logEntry.LogLevel is LogLevel.Error or LogLevel.Critical)
        {
            var threadId = Environment.CurrentManagedThreadId;
            var skip = 7;
            MethodBase caller;
            do
            {
                var frame = new StackFrame(skip);
                var method = frame.GetMethod();
                var methodName = method?.Name ?? "";
                if (methodName.EndsWith("LoggingExtensions") || methodName.EndsWith("Logging") || methodName.EndsWith("LogError") || methodName.EndsWith("LogCritical") || methodName.EndsWith("LogNull"))
                {
                    skip++;
                    continue;
                }

                caller = method!;
                break;
            } while (true);

            textWriter.Write(" [thread ");
            textWriter.Write(threadId);
            textWriter.Write(" at ");
            textWriter.Write(caller.DeclaringType?.FullName);
            textWriter.Write(".");
            textWriter.Write(caller.Name);
            textWriter.Write("]");
        }
#endif

        if (logEntry.Exception != null)
        {
            textWriter.Write(" ");
            textWriter.Write(logEntry.Exception);
            
            var inner = logEntry.Exception.InnerException;
            while (inner != null)
            {
                textWriter.Write(" --INNER--> ");
                textWriter.Write(inner);
                inner = inner.InnerException;
            }
        }
    }
}