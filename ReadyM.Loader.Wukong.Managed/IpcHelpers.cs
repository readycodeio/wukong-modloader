using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using ReadyM.Loader.Wukong.Bootstrap;

namespace EmbedCSharpLoader.Managed;

/// <summary>
/// Copied from the mod. TODO: Shared project
/// </summary>
public class IpcHelpers
{
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern uint GetEnvironmentVariable(string lpName, StringBuilder lpBuffer, uint nSize);

    private static string? GetEnvironmentVariable(string variable)
    {
        const int initialSize = 512;
        StringBuilder buffer = new(initialSize);

        var size = GetEnvironmentVariable(variable, buffer, (uint)buffer.Capacity);
        if (size == 0)
        {
            var error = Marshal.GetLastWin32Error();
            if (error == 203) // ERROR_ENVVAR_NOT_FOUND
                return null;
            if (error == 0 && buffer.Length == 0)
                return null;

            throw new System.ComponentModel.Win32Exception(error);
        }

        if (size > buffer.Capacity)
        {
            buffer = new StringBuilder((int)size);
            GetEnvironmentVariable(variable, buffer, size);
        }

        return buffer.ToString();
    }

    public static Dictionary<string, string> ReadIpcHandshakeFile()
    {
        var tempDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "ReadyM.Launcher");
        var filePath = Path.Combine(tempDir, "wukong_handshake.env");

        if (!File.Exists(filePath))
        {
            Log.Error($"Handshake file not found at {filePath}. Launch the game from the ReadyM Launcher.");
            return [];
        }

        Log.Debug($"Reading handshake file: {filePath}");
        var lines = File.ReadAllLines(filePath);
        var data = new Dictionary<string, string>();

        // format is .env KEY=VALUE
        var regex = new Regex(@"^(?<key>[^=]+)=(?<value>.*)$", RegexOptions.Compiled | RegexOptions.IgnoreCase);
        foreach (var line in lines)
        {
            var match = regex.Match(line);
            if (match.Success)
            {
                var key = match.Groups["key"].Value.Trim();
                var value = match.Groups["value"].Value.Trim();
                data[key] = value;
                Log.Debug($"Parsed {key}={value}");
            }
            else
            {
                Log.Warn($"Failed to parse line: {line}");
            }
        }

        return data;
    }
}