using SharpDX.XInput;

namespace CSharpModBase.Input;

public static class GamePadUtils
{
    private static readonly Controller controller = new(UserIndex.One);

    public static bool GetGamePadButtons(out GamePadButton flags)
    {
        flags = GamePadButton.None;
        if (controller.GetState(out State state))
        {
            Gamepad gamepad = state.Gamepad;
            flags = (GamePadButton)gamepad.Buttons;
            if (gamepad.LeftTrigger > 100) flags |= GamePadButton.LeftTrigger;
            if (gamepad.RightTrigger > 100) flags |= GamePadButton.RightTrigger;
            return true;
        }
        return false;
    }
}