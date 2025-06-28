using System.Reflection;
using Microsoft.Extensions.Logging;
using Microsoft.Win32.SafeHandles;
using ReadyM.Loader.Wukong.Bootstrap.Logging;
using UnrealEngine.Runtime;

namespace ReadyM.Loader.Wukong.Bootstrap;

// ReSharper disable once UnusedType.Global
public static class EntryPoint
{
    private static LoggerFactoryProvider _provider = null!;
    private static ILoggerFactory _loggerFactory = null!;
    private static ILogger _logger = null!;
    
    private static Bootstrapper _bootstrapper = null!;
    
    private static Assembly? _loaderAssembly;
    private static Type? _managedEntryPoint;

    // ReSharper disable once UnusedMember.Global
    public static void InitLogging(long handle)
    {
        var ptr = new IntPtr(handle);
        var logFileHandle = new SafeFileHandle(ptr, ownsHandle: false);

        _provider = new LoggerFactoryProvider(Guid.NewGuid(), logFileHandle);
        Log.Provider = _provider;
        
        _loggerFactory = _provider.CreateLoggerFactory(true, true);
        _logger = _loggerFactory.CreateLogger("Bootstrap");

        _logger.LogDebug("Logging initialized.");
    }
 
    // ReSharper disable once UnusedMember.Global
    public static void Init()
    {
        _logger.LogDebug("Bootstrapping...");

        _bootstrapper = new Bootstrapper(_logger);
        
        try
        {
            _bootstrapper.Setup();
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error while bootstrapping");
            return;
        }

        try
        {
            _loaderAssembly = Assembly.LoadFrom("CSharpLoader\\ReadyM.Loader.Wukong.Managed.dll");
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error while opening loader assembly");
            return;
        }

        _managedEntryPoint = _loaderAssembly.GetType("ReadyM.Loader.Wukong.Managed.EntryPoint");
        if (_managedEntryPoint == null)
        {
            _logger.LogError("Could not find entry point");
            return;
        }
        
        var initMethod = _managedEntryPoint.GetMethod("Init");
        if (initMethod == null)
        {
            _logger.LogError("Could not find Init method in entry point");
            return;
        }
        
        try
        {
            initMethod.Invoke(null, null);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error while invoking Init method");
        }

        _logger.LogDebug("Bootstrapping complete.");
        
        FCoreDelegates.OnExit.Bind(DeInit);
    }

    public static void DeInit()
    {
        if (_managedEntryPoint != null)
        {
            var deInitMethod = _managedEntryPoint.GetMethod("DeInit");
            if (deInitMethod == null)
            {
                _logger.LogError("Could not find DeInit method in entry point");
                return;
            }
        
            try
            {
                deInitMethod.Invoke(null, null);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error while invoking DeInit method");
            }
        }
        
        _provider.Dispose();
    }
}
