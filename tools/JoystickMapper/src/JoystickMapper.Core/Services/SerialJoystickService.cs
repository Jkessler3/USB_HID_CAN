using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using JoystickMapper.Core.Models;

namespace JoystickMapper.Core.Services;

public sealed class SerialJoystickService
{
    private static readonly Regex HidJoyRegex = new(@"HID JOY.*?ax=\[(?<axes>[^\]]+)\].*?btn=(?<btn>[0-9a-fA-F]+)", RegexOptions.Compiled);

    public IReadOnlyList<string> ListPorts()
    {
        var ports = new List<string>();
        for (var i = 1; i <= 64; i++)
        {
            var name = $"COM{i}";
            if (QueryDosDevice(name, null, 0) != 0 || Marshal.GetLastWin32Error() != 2)
                ports.Add(name);
        }
        return ports;
    }

    public JoystickSample? TryParseDebugLine(string line)
    {
        var match = HidJoyRegex.Match(line);
        if (!match.Success)
            return null;

        var sample = new JoystickSample
        {
            StatusFlags = Can302StatusFrame.JoystickPresent | Can302StatusFrame.HidSampleValid,
            Buttons = new bool[32],
            UpdatedAt = DateTimeOffset.Now
        };

        var axisNames = new[] { "X", "Y", "Z", "Rx" };
        var values = match.Groups["axes"].Value.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
        for (var i = 0; i < values.Length && i < axisNames.Length; i++)
        {
            if (!int.TryParse(values[i], out var value))
                continue;
            sample.Axes[axisNames[i]] = new AxisState { Name = axisNames[i], Raw = value, Normalized = Math.Clamp(value, -1000, 1000) };
        }

        if (uint.TryParse(match.Groups["btn"].Value, System.Globalization.NumberStyles.HexNumber, null, out var buttons))
        {
            for (var i = 0; i < sample.Buttons.Length; i++)
                sample.Buttons[i] = (buttons & (1U << i)) != 0;
        }

        return sample;
    }

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern uint QueryDosDevice(string lpDeviceName, System.Text.StringBuilder? lpTargetPath, int ucchMax);
}
