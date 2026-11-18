namespace CANaerospaceJoystickMapper.Core.Models;

public sealed class AxisMapping
{
    public bool Enabled { get; set; } = true;
    public string Source { get; set; } = "X";
    public string ParameterName { get; set; } = "Joystick Axis X/Y";
    public int CanIdDecimal { get; set; } = 1500;
    public string CanIdHex => $"0x{CanIdDecimal:X3}";
    public string DataType { get; set; } = "USHORT2";
    public string DataTypeCode => $"0x{CANaerospaceDataType.FromName(DataType).Code:X2}";
    public int DataBytePosition { get; set; } = 4;
    public int BitLength { get; set; } = 16;
    public bool Signed { get; set; }
    public bool LittleEndian { get; set; }
    public double Scale { get; set; } = 1.0;
    public double Offset { get; set; }
    public bool Invert { get; set; }
    public int Deadzone { get; set; }
    public int CenterOffset { get; set; }
    public int InputMin { get; set; } = -32768;
    public int InputMax { get; set; } = 32767;
    public int OutputMin { get; set; }
    public int OutputMax { get; set; } = 65535;
    public string Units { get; set; } = "normalized unsigned";
    public string Notes { get; set; } = "CANaerospace parameter data is big-endian in bytes 4..7.";

    public int Apply(int raw)
    {
        var centered = (long)raw + CenterOffset;
        if (Math.Abs(centered) < Deadzone)
            centered = 0;

        var clamped = Math.Clamp(centered, InputMin, InputMax);
        double fraction = (clamped - InputMin) / ((double)InputMax - InputMin);
        if (Invert) fraction = 1 - fraction;
        double output = (OutputMin + fraction * (OutputMax - OutputMin)) * Scale + Offset;
        return (int)Math.Round(Math.Clamp(output, Math.Min(OutputMin, OutputMax), Math.Max(OutputMin, OutputMax)));
    }

    public IReadOnlyList<string> Validate()
    {
        var errors = new List<string>();
        if (Source is not ("X" or "Y" or "Z" or "Rx" or "Ry" or "Rz" or "Slider" or "Dial"))
            errors.Add("Select X, Y, Z, Rx, Ry, Rz, Slider or Dial.");
        if (CanIdDecimal <= 0 || CanIdDecimal > 0x7FF)
            errors.Add("CAN ID must be a standard 11-bit identifier.");
        if (string.IsNullOrWhiteSpace(ParameterName))
            errors.Add("A parameter name is required.");
        if (CANaerospaceDataType.FromName(DataType) != CANaerospaceDataType.UShort2 || Signed || LittleEndian || BitLength != 16)
            errors.Add("Axes require unsigned big-endian USHORT2 with 16-bit lanes.");
        if (DataBytePosition is not (4 or 6))
            errors.Add("An axis must start at byte 4 or 6.");
        if (OutputMin is < 0 or > 65535 || OutputMax is < 0 or > 65535)
            errors.Add("USHORT2 output bounds must fit 0..65535.");
        if (!double.IsFinite(Scale) || !double.IsFinite(Offset))
            errors.Add("Scale and offset must be finite.");
        if (InputMin >= InputMax)
            errors.Add("Input min must be less than input max.");
        if (Deadzone < 0)
            errors.Add("Deadzone cannot be negative.");
        return errors;
    }

    public static List<AxisMapping> CreateDefaults() =>
    [
        new() { Source = "X", ParameterName = "Joystick Axis X/Y", CanIdDecimal = 1500, DataBytePosition = 4 },
        new() { Source = "Y", ParameterName = "Joystick Axis X/Y", CanIdDecimal = 1500, DataBytePosition = 6 },
        new() { Source = "Z", ParameterName = "Joystick Axis Z/Rx", CanIdDecimal = 1501, DataBytePosition = 4 },
        new() { Source = "Rx", ParameterName = "Joystick Axis Z/Rx", CanIdDecimal = 1501, DataBytePosition = 6 },
        new() { Source = "Ry", ParameterName = "Joystick Axis Ry/Rz", CanIdDecimal = 1502, DataBytePosition = 4 },
        new() { Source = "Rz", ParameterName = "Joystick Axis Ry/Rz", CanIdDecimal = 1502, DataBytePosition = 6 },
        new() { Source = "Slider", ParameterName = "Joystick Axis Slider/Dial", CanIdDecimal = 1503, DataBytePosition = 4 },
        new() { Source = "Dial", ParameterName = "Joystick Axis Slider/Dial", CanIdDecimal = 1503, DataBytePosition = 6 },
    ];
}
