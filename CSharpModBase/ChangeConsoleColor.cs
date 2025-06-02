using System;

namespace CSharpModBase;

public readonly struct ChangeConsoleColor : IDisposable
{
    private readonly ConsoleColor _currentForeground;

    public ChangeConsoleColor(ConsoleColor color)
    {
        _currentForeground = Console.ForegroundColor;
        Console.ForegroundColor = color;
    }

    public void Dispose()
    {
        Console.ForegroundColor = _currentForeground;
    }
}