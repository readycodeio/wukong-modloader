using System;

namespace CSharpModBase;

public static class Log
{
    public static void Info(string message)
        => ReadyM.Loader.Wukong.Bootstrap.Log.Info(message);

    public static void Debug(string message)
        => ReadyM.Loader.Wukong.Bootstrap.Log.Debug(message);

    public static void Warn(string message)
        => ReadyM.Loader.Wukong.Bootstrap.Log.Warn(message);

    public static void WarnIf(bool condition, string message)
        => ReadyM.Loader.Wukong.Bootstrap.Log.WarnIf(condition, message);

    public static void Error(string message)
        => ReadyM.Loader.Wukong.Bootstrap.Log.Error(message);

    public static void Error(Exception exc)
        => ReadyM.Loader.Wukong.Bootstrap.Log.Error(exc);
}
