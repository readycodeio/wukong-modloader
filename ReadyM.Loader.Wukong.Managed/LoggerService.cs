using Microsoft.Extensions.Logging;
using ReadyM.Loader.Wukong.Bootstrap;

namespace ReadyM.Loader.Wukong.Managed;

public class LoggerService
{
    private Thread? _logLoopThread;
    private readonly LoadingPhaseManager _loadingPhaseManager;

    public LoggerService(ILogger logger, LoadingPhaseManager loadingPhaseManager)
    {
        _loadingPhaseManager = loadingPhaseManager;

        _loadingPhaseManager.OnCancel += OnCancel;
    }

    public void StartLogLoop()
    {
        _logLoopThread = new Thread(LogLoop)
        {
            IsBackground = true,
        };
        _logLoopThread.Start();
    }

    private void LogLoop()
    {
        while (!_loadingPhaseManager.IsLoadingCancelled)
        {
            Log.Provider.Flush();
            _loadingPhaseManager.CancellationToken.WaitHandle.WaitOne(100);
        }
    }
    
    private void OnCancel()
    {
        _logLoopThread?.Join();
    }
}