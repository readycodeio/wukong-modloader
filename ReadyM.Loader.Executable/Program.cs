using HarmonyLib;

namespace ReadyM.Loader.Executable
{
    internal class Program
    {
        public static void Main(string[] args)
        {
            var harmony = new Harmony("abc");
        }
    }
}