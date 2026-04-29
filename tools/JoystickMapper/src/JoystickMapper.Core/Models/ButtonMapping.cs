namespace JoystickMapper.Core.Models;

public sealed class ButtonMapping
{
    public bool Enabled { get; set; } = true;
    public int ButtonIndex { get; set; }
    public string DestinationCanId { get; set; } = "0x301";
    public int ByteOffset { get; set; } = 6;
    public int BitIndex { get; set; }
    public bool ActiveHigh { get; set; } = true;

    public byte Mask => (byte)(1 << BitIndex);

    public IReadOnlyList<string> Validate()
    {
        var errors = new List<string>();
        if (ButtonIndex < 0 || ButtonIndex > 63)
            errors.Add("Button index must be 0..63.");
        if (!CanId.TryParse(DestinationCanId, out _))
            errors.Add("Destination CAN ID is invalid.");
        if (ByteOffset < 0 || ByteOffset > 7)
            errors.Add("Byte offset must be 0..7.");
        if (BitIndex < 0 || BitIndex > 7)
            errors.Add("Bit index must be 0..7.");
        return errors;
    }

    public static List<ButtonMapping> CreateDefaults() =>
    [
        new() { ButtonIndex = 0, BitIndex = 0 },
        new() { ButtonIndex = 1, BitIndex = 1 },
        new() { ButtonIndex = 2, BitIndex = 2 },
        new() { ButtonIndex = 3, BitIndex = 3 },
        new() { Enabled = false, ButtonIndex = 4, BitIndex = 4 },
        new() { Enabled = false, ButtonIndex = 5, BitIndex = 5 },
        new() { Enabled = false, ButtonIndex = 6, BitIndex = 6 },
        new() { Enabled = false, ButtonIndex = 7, BitIndex = 7 },
    ];
}
