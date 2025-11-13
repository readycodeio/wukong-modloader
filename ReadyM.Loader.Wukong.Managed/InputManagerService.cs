using CSharpModBase;
using CSharpModBase.Input;

namespace ReadyM.Loader.Wukong.Managed;

public class InputManagerService(InputManager inputManager, LoadingPhaseManager loadingPhaseManager)
{
    private Thread? _inputLoopThread;
    
    public void StartInputLoop()
    {
        inputManager.RegisterBuiltinKeyBind(ModifierKeys.Control, Key.F5, loadingPhaseManager.RequestReloadMods);
        _inputLoopThread = new Thread(InputLoop)
        {
            IsBackground = true,
        };
        _inputLoopThread.Start();
        
        loadingPhaseManager.OnCancel += OnCancel;
        loadingPhaseManager.OnReloadModsRequested += OnReloadModsRequested;
    }

    private void InputLoop()
    {
        while (!loadingPhaseManager.IsLoadingCancelled)
        {
            inputManager.Update();
            loadingPhaseManager.CancellationToken.WaitHandle.WaitOne(10);
        }
    }

    private void OnCancel()
    {
        _inputLoopThread?.Join();
    }
    
    private void OnReloadModsRequested()
    {
        inputManager.Clear();
    }
}