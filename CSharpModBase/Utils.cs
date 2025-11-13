using System;
using UnrealEngine.Runtime;

namespace CSharpModBase;

public static class Utils
{
    public static void TryRun(Action aciton)
    {
        try
        {
            aciton();
        }
        catch (Exception e)
        {
            Log.Error(e);
        };
    }

    public static void TryRunOnGameThread(Action aciton)
    {
        FThreading.RunOnGameThread(() =>
        {
            try
            {
                aciton();
            }
            catch (Exception e)
            {
                Log.Error(e);
            }
        });
    }
}
