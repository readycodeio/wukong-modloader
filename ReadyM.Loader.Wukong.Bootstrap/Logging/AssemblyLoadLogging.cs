using System.Reflection;
using Microsoft.Extensions.Logging;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

/// <summary>
/// Assembly load failures are the hardest thing to diagnose from a player's log, because the part that says
/// what actually went wrong is never on the outer exception: it is on the inner exception chain, on
/// <see cref="ReflectionTypeLoadException.LoaderExceptions"/>, or in the fusion log. ILogger writes none of
/// those by default, so a failed load shows up as a single generic line.
/// </summary>
public static class AssemblyLoadLogging
{
    private const int MaxExceptionDepth = 8;
    private const int MaxLoaderExceptions = 25;

    /// <summary>
    /// Logs everything useful hanging off <paramref name="ex"/>. Call this right after logging the exception
    /// itself, e.g.
    /// <code>
    /// logger.LogError(ex, "Assembly.LoadFrom failed for {Path}", path);
    /// logger.LogAssemblyLoadDetail(ex);
    /// </code>
    /// The outer LogError is left to the caller so that the message template stays a literal (CA2254).
    /// </summary>
    public static void LogAssemblyLoadDetail(this ILogger logger, Exception ex)
        => LogExceptionDetail(logger, ex, 0);

    private static void LogExceptionDetail(ILogger logger, Exception ex, int depth)
    {
        if (depth >= MaxExceptionDepth)
        {
            logger.LogError("Exception chain truncated at depth {Depth}", depth);
            return;
        }

        switch (ex)
        {
            case ReflectionTypeLoadException typeLoadEx:
                LogLoaderExceptions(logger, typeLoadEx, depth);
                break;

            // NOTE: null on Mono unless the fusion log is explicitly enabled, but free to ask for.
            case FileNotFoundException { FusionLog: { } fileNotFoundFusionLog }:
                logger.LogError("Fusion log: {FusionLog}", fileNotFoundFusionLog);
                break;

            case FileLoadException { FusionLog: { } fileLoadFusionLog }:
                logger.LogError("Fusion log: {FusionLog}", fileLoadFusionLog);
                break;
        }

        if (ex.InnerException == null)
            return;

        logger.LogError(ex.InnerException, "Inner exception (depth {Depth}):", depth + 1);
        LogExceptionDetail(logger, ex.InnerException, depth + 1);
    }

    private static void LogLoaderExceptions(ILogger logger, ReflectionTypeLoadException typeLoadEx, int depth)
    {
        var loaderExceptions = typeLoadEx.LoaderExceptions;
        if (loaderExceptions == null)
            return;

        // Every entry here is one type that could not be loaded. This is where a missing or mismatched
        // dependency assembly finally names itself.
        logger.LogError("ReflectionTypeLoadException carries {Count} loader exception(s)", loaderExceptions.Length);

        var reported = 0;
        var seen = new HashSet<string>(StringComparer.Ordinal);

        foreach (var loaderEx in loaderExceptions)
        {
            if (loaderEx == null)
                continue;

            // The same missing dependency usually fails for dozens of types, so collapse duplicates.
            if (!seen.Add($"{loaderEx.GetType().FullName}: {loaderEx.Message}"))
                continue;

            if (reported++ >= MaxLoaderExceptions)
            {
                logger.LogError("Further loader exceptions suppressed after {Count} distinct entries", MaxLoaderExceptions);
                break;
            }

            logger.LogError(loaderEx, "Loader exception:");
            LogExceptionDetail(logger, loaderEx, depth + 1);
        }
    }
}
