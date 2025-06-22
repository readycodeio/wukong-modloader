namespace ReadyM.Loader.Wukong.Bootstrap;

public static class Log
{
    private readonly struct ChangeConsoleColor : IDisposable
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
    
    private static string DateTimeString => DateTime.Now.ToString("MM-dd HH:mm:ss.fff");  // .fff
    private static readonly StreamWriter LogFile = File.CreateText(Path.Combine(ModLoaderSettings.BaseDir, "CSharpLog.txt"));

    public static void Flush()
    {
        LogFile.Flush();
    }
    
    public static void Info(string message)
    {
        var text = $"{DateTimeString} [I] {message}";
        Console.WriteLine(text);
        LogFile.WriteLine(text);
    }

    public static void Debug(string message)
    {
        using (new ChangeConsoleColor(ConsoleColor.Gray))
        {
            var text = $"{DateTimeString} [D] {message}";
            Console.WriteLine(text);
            LogFile.WriteLine(text);
        }
    }

    public static void Warn(string message)
    {
        using (new ChangeConsoleColor(ConsoleColor.Yellow))
        {
            var text = $"{DateTimeString} [W] {message}";
            Console.WriteLine(text);
            LogFile.WriteLine(text);
        }
    }

    public static void WarnIf(bool condition, string message)
    {
        if (condition)
            Warn(message);
    }

    public static void Error(string message)
    {
        using (new ChangeConsoleColor(ConsoleColor.Red))
        {
            var text = $"{DateTimeString} [E] {message}";
            Console.Error.WriteLine(text);
            LogFile.WriteLine(text);
        }
    }

    public static void Error(Exception exc)
    {
        Error(exc.ToString());
    }
}
