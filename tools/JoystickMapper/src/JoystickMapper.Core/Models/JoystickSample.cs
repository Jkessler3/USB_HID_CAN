namespace JoystickMapper.Core.Models;

public sealed class JoystickSample
{
    public Dictionary<string, AxisState> Axes { get; set; } = new();
    public bool[] Buttons { get; set; } = new bool[32];
    public int Hat { get; set; } = -1;
    public byte StatusFlags { get; set; }
    public byte SampleCounter { get; set; }
    public byte CanFrameCounter { get; set; }
    public uint LastReportMs { get; set; }
    public DateTimeOffset UpdatedAt { get; set; } = DateTimeOffset.Now;

    public bool JoystickPresent => (StatusFlags & Can302StatusFrame.JoystickPresent) != 0;
    public bool HidSampleValid => (StatusFlags & Can302StatusFrame.HidSampleValid) != 0;
    public bool DataStale => (StatusFlags & Can302StatusFrame.DataStale) != 0;
    public bool TxBackpressureSeen => (StatusFlags & Can302StatusFrame.TxBackpressureSeen) != 0;
    public bool HidParseError => (StatusFlags & Can302StatusFrame.HidParseError) != 0;
    public bool UsbDisconnected => (StatusFlags & Can302StatusFrame.UsbDisconnected) != 0;
}

public sealed class AxisState
{
    public string Name { get; set; } = "";
    public int Raw { get; set; }
    public int Normalized { get; set; }
    public double Percent => Math.Clamp((Normalized + 1000) / 2000.0, 0, 1);
}
