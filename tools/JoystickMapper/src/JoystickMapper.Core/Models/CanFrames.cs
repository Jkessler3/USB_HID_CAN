namespace JoystickMapper.Core.Models;

public sealed class Can301ControlFrame
{
    public const int Id = 0x301;

    public short Steer { get; init; }
    public short Throttle { get; init; }
    public sbyte Aux0 { get; init; }
    public sbyte Aux1 { get; init; }
    public byte Buttons { get; init; }
    public byte SampleCounter { get; init; }
}

public sealed class Can302StatusFrame
{
    public const int Id = 0x302;

    public const byte JoystickPresent = 0x01;
    public const byte HidSampleValid = 0x02;
    public const byte DataStale = 0x04;
    public const byte TxBackpressureSeen = 0x08;
    public const byte HidParseError = 0x10;
    public const byte UsbDisconnected = 0x20;

    public byte StatusFlags { get; init; }
    public byte AxisCount { get; init; }
    public byte SampleCounter { get; init; }
    public byte FrameCounter { get; init; }
    public uint LastReportMs { get; init; }

    public IReadOnlyList<string> FlagNames
    {
        get
        {
            var names = new List<string>();
            if ((StatusFlags & JoystickPresent) != 0) names.Add("Joystick Present");
            if ((StatusFlags & HidSampleValid) != 0) names.Add("HID Sample Valid");
            if ((StatusFlags & DataStale) != 0) names.Add("Data Stale");
            if ((StatusFlags & TxBackpressureSeen) != 0) names.Add("TX Backpressure Seen");
            if ((StatusFlags & HidParseError) != 0) names.Add("HID Parse Error");
            if ((StatusFlags & UsbDisconnected) != 0) names.Add("USB Disconnected");
            return names;
        }
    }
}

public sealed class DecodedCanFrame
{
    public int CanId { get; init; }
    public byte[] Payload { get; init; } = [];
    public string FrameType { get; init; } = "Unknown";
    public string Summary { get; init; } = "";
    public bool IsValid { get; init; }
    public string? Error { get; init; }
    public Can301ControlFrame? Control { get; init; }
    public Can302StatusFrame? Status { get; init; }
}

public static class CanId
{
    public static bool TryParse(string? value, out int id)
    {
        id = 0;
        if (string.IsNullOrWhiteSpace(value))
            return false;
        value = value.Trim();
        if (value.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            return int.TryParse(value[2..], System.Globalization.NumberStyles.HexNumber, null, out id);
        return int.TryParse(value, out id);
    }
}
