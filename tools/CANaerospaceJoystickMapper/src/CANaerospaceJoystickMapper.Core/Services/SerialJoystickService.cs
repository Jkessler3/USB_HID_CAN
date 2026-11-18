using System.Globalization;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using CANaerospaceJoystickMapper.Core.Models;

namespace CANaerospaceJoystickMapper.Core.Services;

public sealed class SerialJoystickService
{
    private static readonly string[] AxisNames = ["X", "Y", "Z", "Rx", "Ry", "Rz", "Slider", "Dial"];
    private static readonly Regex HidJoyRegex = new(
        @"\bHID JOY\b.*?\bax=\[(?<axes>[^\]]*)\]\s+btn=(?<btn>[0-9a-fA-F]{1,16})\s+bc=(?<count>\d+)\s+hat=(?<hat>-?\d+)(?:\s|$)",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    public IReadOnlyList<string> ListPorts()
    {
        var ports = new List<string>();
        var device = new System.Text.StringBuilder(1024);
        for (var i = 1; i <= 64; i++)
        {
            var name = $"COM{i}";
            if (QueryDosDevice(name, device, device.Capacity) != 0)
                ports.Add(name);
        }
        return ports;
    }

    public JoystickSample? TryParseDebugLine(string line)
    {
        var match = HidJoyRegex.Match(line);
        if (!match.Success
            || !int.TryParse(match.Groups["count"].Value, out int count) || count is < 0 or > 64
            || !int.TryParse(match.Groups["hat"].Value, out int hat) || hat is < -1 or > 7
            || !ulong.TryParse(match.Groups["btn"].Value, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var bits))
            return null;

        string axisText = match.Groups["axes"].Value.Trim();
        string[] values = axisText.Length == 0 ? [] : axisText.Split(',', StringSplitOptions.TrimEntries);
        if (values.Length > AxisNames.Length) return null;

        // Older firmware logged four axes and a 32-bit button word.
        var sample = new JoystickSample { Buttons = new bool[Math.Min(count, match.Groups["btn"].Length * 4)], Hat = hat };
        for (int i = 0; i < values.Length; i++)
        {
            if (!short.TryParse(values[i], NumberStyles.Integer, CultureInfo.InvariantCulture, out short raw))
                return null;
            var name = AxisNames[i];
            sample.Axes[name] = new AxisState { Name = name, Raw = raw, Normalized = (int)Math.Round(raw * 1000.0 / (raw < 0 ? 32768 : 32767)) };
        }
        for (int i = 0; i < sample.Buttons.Length; i++)
            sample.Buttons[i] = (bits & (1UL << i)) != 0;
        return sample;
    }

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern uint QueryDosDevice(string lpDeviceName, System.Text.StringBuilder lpTargetPath, int ucchMax);
}
