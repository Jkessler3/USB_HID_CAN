namespace CANaerospaceJoystickMapper.Core.Models;

public sealed class ButtonMapping
{
    public bool Enabled { get; set; } = true;
    public int ButtonIndex { get; set; }
    public string ParameterName { get; set; } = "Joystick Buttons 0..31";
    public int CanIdDecimal { get; set; } = 1504;
    public string CanIdHex => $"0x{CanIdDecimal:X3}";
    public string DataType { get; set; } = "BLONG";
    public int BitIndex { get; set; }
    public bool ActiveHigh { get; set; } = true;
    public string Notes { get; set; } = "Button 0 is bit 0 of the 32-bit BLONG value; the BLONG is serialized big-endian on the CAN bus.";

    public uint Mask => 1u << BitIndex;

    public IReadOnlyList<string> Validate()
    {
        var errors = new List<string>();
        if (string.IsNullOrWhiteSpace(ParameterName))
            errors.Add("A parameter name is required.");
        if (CANaerospaceDataType.FromName(DataType) != CANaerospaceDataType.BLong)
            errors.Add("Button frames require the historical BLONG encoding.");
        if (ButtonIndex < 0 || ButtonIndex > 63)
            errors.Add("Button index must be 0..63.");
        if (CanIdDecimal <= 0 || CanIdDecimal > 0x7FF)
            errors.Add("CAN ID must be a standard 11-bit identifier.");
        if (BitIndex < 0 || BitIndex > 31)
            errors.Add("Bit index must be 0..31 inside the CANaerospace BLONG value.");
        return errors;
    }

    public static List<ButtonMapping> CreateDefaults() =>
    [
        .. Enumerable.Range(0, 32).Select(i => new ButtonMapping { ButtonIndex = i, BitIndex = i, CanIdDecimal = 1504, ParameterName = "Joystick Buttons 0..31" }),
        .. Enumerable.Range(32, 32).Select(i => new ButtonMapping { ButtonIndex = i, BitIndex = i - 32, CanIdDecimal = 1505, ParameterName = "Joystick Buttons 32..63" }),
    ];
}
