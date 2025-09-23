using HarmonyLib;
using PreludeLib.CompileTime.Backend.WeaverCallback;
using PreludeLib.CompileTime.Public;
using PreludeLib.Runtime.Backend.HarmonyDetour;
using PreludeLib.Runtime.Backend.WeaverCallback;
using PreludeLib.Runtime.Public;
using SharpDX.XInput;

namespace ReadyM.Loader.Executable
{
    internal class Program
    {
        public static void Main(string[] args)
        {
            var boolFlag = args.Length > 0;
            
            var harmony = new Harmony("abc");
            var controller = new Controller();
            var compileTimeBackend = new CompileTimeWeaverBackend(null!);
            var compileTimePrelude = new CompileTimePrelude(compileTimeBackend, null!);
            var runtimePreludeBackend1 = new RuntimeHarmonyBackend(null!);
            var runtimePreludeBackend2 = new RuntimeWeaverBackend(null!);
            var runtimePrelude = new RuntimePrelude(boolFlag ? runtimePreludeBackend1 : runtimePreludeBackend2, null!);
        }
    }
}