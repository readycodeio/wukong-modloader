 using Microsoft.Extensions.Logging;

namespace ReadyM.Loader.Wukong.Managed;

public class LoadingPhaseManager
{
    private readonly ILogger _logger;
    
    private readonly CancellationTokenSource _cancellationTokenSource = new();
    private CancellationToken _cancellationToken;

    public event Action? OnCancel;
    public event Action? OnReloadModsRequested;

    public  CancellationToken CancellationToken
        => _cancellationToken;

    public bool IsLoadingCancelled
        => _cancellationToken.IsCancellationRequested;
    
    public LoadingPhaseManager(ILogger logger)
    {
        _logger = logger;
        
        _cancellationToken = _cancellationTokenSource.Token;
        _cancellationToken.Register(() => OnCancel?.Invoke());
    }

    public void CancelLoading()
    {
        _logger.LogDebug("Cancelling in progress operations...");
        _cancellationTokenSource.Cancel();
        _logger.LogDebug("All operations cancelled.");
    }

    public void RequestReloadMods()
    {
        _logger.LogDebug("Requesting mod reload...");
        OnReloadModsRequested?.Invoke();
        _logger.LogDebug("Reload complete.");
    }
}