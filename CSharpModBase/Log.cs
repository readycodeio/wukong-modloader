using System;
using System.IO;

namespace CSharpModBase;
    
public static class Log
{
    private static string DateTimeString => DateTime.Now.ToString("MM-dd HH:mm:ss.fff");  // .fff
    private static readonly StreamWriter LogFile = File.CreateText(Path.Combine(Common.BaseDir, "CSharpLog.txt"));

    public static void Loop()
    {
        while (true)
        {
            LogFile.Flush();
            System.Threading.Thread.Sleep(100);
        }
    }
    
    public static void Info(string message)
    {
        var text = $"{DateTimeString} [I] {message}";
        Console.WriteLine(text);
        LogFile.WriteLine(text);
    }

    public static void Debug(string message)
    {
        using (var color = new ChangeConsoleColor(ConsoleColor.Gray))
        {
            var text = $"{DateTimeString} [D] {message}";
            Console.WriteLine(text);
            LogFile.WriteLine(text);
        }
    }

    public static void Warn(string message)
    {
        using (var color = new ChangeConsoleColor(ConsoleColor.Yellow))
        {
            var text = $"{DateTimeString} [W] {message}";
            Console.WriteLine(text);
            LogFile.WriteLine(text);
        }
    }

    public static void WarnIf(bool condition, string message)
    {
        if (condition) {
            Warn(message);
        }
    }

    public static void Error(string message)
    {
        using (var color = new ChangeConsoleColor(ConsoleColor.Red))
        {
            string text = $"{DateTimeString} [E] {message}";
            Console.Error.WriteLine(text);
            LogFile.WriteLine(text);
        }
    }

    public static void Error(Exception e)
    {
        Error(e.Message);
        Error(e.StackTrace);
    }
}
