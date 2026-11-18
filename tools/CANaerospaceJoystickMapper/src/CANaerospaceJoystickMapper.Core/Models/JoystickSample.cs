namespace CANaerospaceJoystickMapper.Core.Models;

public sealed class JoystickSample
{
    public Dictionary<string, AxisState> Axes { get; set; } = new();
    public bool[] Buttons { get; set; } = new bool[64];
    public int Hat { get; set; } = -1;
    public byte StatusFlags { get; set; }
    public byte SampleCounter { get; set; }
    public byte CanFrameCounter { get; set; }
    public uint LastReportMs { get; set; }
    public DateTimeOffset UpdatedAt { get; set; } = DateTimeOffset.Now;

    public bool JoystickPresent => (StatusFlags & CANaerospaceServiceCode.JoystickNotPresent) == 0;
    public bool HidSampleValid => (StatusFlags & (CANaerospaceServiceCode.DataStale | CANaerospaceServiceCode.HidParseError | CANaerospaceServiceCode.UsbDisconnected)) == 0;
    public bool DataStale => (StatusFlags & CANaerospaceServiceCode.DataStale) != 0;
    public bool TxBackpressureSeen => (StatusFlags & CANaerospaceServiceCode.TxBackpressureSeen) != 0;
    public bool HidParseError => (StatusFlags & CANaerospaceServiceCode.HidParseError) != 0;
    public bool UsbDisconnected => (StatusFlags & CANaerospaceServiceCode.UsbDisconnected) != 0;
}

public sealed class AxisState
{
    public string Name { get; set; } = "";
    public int Raw { get; set; }
    public int Normalized { get; set; }
    public double Percent => Math.Clamp((Normalized + 1000) / 2000.0, 0, 1);
}
