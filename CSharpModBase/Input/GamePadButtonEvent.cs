using System;

namespace CSharpModBase.Input;

public class GamePadButtonEvent : EventArgs
{
    public GamePadButton Button { get; set; }
}