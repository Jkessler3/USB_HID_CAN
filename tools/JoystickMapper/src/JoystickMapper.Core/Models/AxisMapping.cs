namespace JoystickMapper.Core.Models;

public sealed class AxisMapping
{
    public bool Enabled { get; set; } = true;
    public string Source { get; set; } = "X";
    public string DestinationCanId { get; set; } = "0x301";
    public string DestinationSignal { get; set; } = "AxisX";
    public int ByteOffset { get; set; }
    public int BitLength { get; set; } = 16;
    public bool Signed { get; set; } = true;
    public bool LittleEndian { get; set; } = true;
    public bool Invert { get; set; }
    public int Deadzone { get; set; } = 512;
    public int CenterOffset { get; set; }
    public int InputMin { get; set; } = -32768;
    public int InputMax { get; set; } = 32767;
    public int OutputMin { get; set; } = -1000;
    public int OutputMax { get; set; } = 1000;

    public int Apply(int raw)
    {
        var centered = raw + CenterOffset;
        if (Math.Abs(centered) < Deadzone)
            centered = 0;

        var clamped = Math.Clamp(centered, InputMin, InputMax);
        var inputSpan = InputMax - InputMin;
        if (inputSpan == 0)
            return OutputMin;

        var scaled = (int)Math.Round(((double)(clamped - InputMin) / inputSpan) * (OutputMax - OutputMin) + OutputMin);
        if (Invert)
            scaled = -scaled;
        return Math.Clamp(scaled, Math.Min(OutputMin, OutputMax), Math.Max(OutputMin, OutputMax));
    }

    public IReadOnlyList<string> Validate()
    {
        var errors = new List<string>();
        if (string.IsNullOrWhiteSpace(Source))
            errors.Add("Source is required.");
        if (!CanId.TryParse(DestinationCanId, out _))
            errors.Add("Destination CAN ID is invalid.");
        if (ByteOffset < 0 || ByteOffset > 7)
            errors.Add("Byte offset must be 0..7.");
        if (BitLength <= 0 || BitLength > 64)
            errors.Add("Bit length must be 1..64.");
        if (InputMin >= InputMax)
            errors.Add("Input min must be less than input max.");
        if (Deadzone < 0)
            errors.Add("Deadzone cannot be negative.");
        return errors;
    }

    public static List<AxisMapping> CreateDefaults() =>
    [
        new() { Source = "X", DestinationSignal = "Steer", ByteOffset = 0, Invert = false },
        new() { Source = "Y", DestinationSignal = "Throttle", ByteOffset = 2, Invert = true },
        new() { Enabled = false, Source = "Z", DestinationSignal = "Aux0", ByteOffset = 4, BitLength = 8 },
        new() { Enabled = false, Source = "Rx", DestinationSignal = "Aux1", ByteOffset = 5, BitLength = 8 },
        new() { Enabled = false, Source = "Ry", DestinationSignal = "Ry", ByteOffset = 0 },
        new() { Enabled = false, Source = "Rz", DestinationSignal = "Rz", ByteOffset = 0 },
        new() { Enabled = false, Source = "Slider", DestinationSignal = "Slider", ByteOffset = 0 },
        new() { Enabled = false, Source = "Dial", DestinationSignal = "Dial", ByteOffset = 0 },
    ];
}
