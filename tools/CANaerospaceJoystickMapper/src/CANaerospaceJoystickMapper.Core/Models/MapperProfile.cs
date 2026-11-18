using System.Text.Json.Serialization;

namespace CANaerospaceJoystickMapper.Core.Models;

public sealed class MapperProfile
{
    public int Version { get; set; } = 1;
    public string Protocol { get; set; } = "CANaerospace";
    public string ProfileName { get; set; } = "Default CANaerospace Joystick Mapping";
    public byte NodeId { get; set; } = 1;
    public string IdentifierFormat { get; set; } = "decimal-and-hex";
    public DeviceInfo Device { get; set; } = new();
    public CANaerospaceIdentifierAssignment IdentifierAssignment { get; set; } = new();
    public CANaerospaceServiceCodePolicy ServiceCodePolicy { get; set; } = new();
    public List<CANaerospaceParameter> Parameters { get; set; } = CANaerospaceParameter.CreateDefaults();
    public List<AxisMapping> Axes { get; set; } = AxisMapping.CreateDefaults();
    public List<ButtonMapping> Buttons { get; set; } = ButtonMapping.CreateDefaults();

    [JsonIgnore]
    public string Summary => $"{ProfileName} ({Axes.Count(a => a.Enabled)} axes, {Buttons.Count(b => b.Enabled)} buttons)";

    public static MapperProfile CreateDefault() => new();

    public IReadOnlyDictionary<int, CANaerospaceDataType> GetPreviewParameterTypes() =>
        new PreviewLayout(this).Frames.ToDictionary(f => f.Id, f => f.Type);

    public IReadOnlyList<string> Validate()
    {
        var errors = new List<string>();
        if (Version <= 0)
            errors.Add("Profile version must be positive.");
        if (string.IsNullOrWhiteSpace(ProfileName))
            errors.Add("Profile name is required.");
        if (!string.Equals(Protocol, "CANaerospace", StringComparison.OrdinalIgnoreCase))
            errors.Add("Protocol must be CANaerospace.");
        if (NodeId == 0)
            errors.Add("Node-ID 0 is reserved and cannot be the transmitting node default.");

        errors.AddRange(new PreviewLayout(this).Errors);

        return errors;
    }
}

public sealed class DeviceInfo
{
    public string Name { get; set; } = "Generic HID Joystick";
    public string? Vid { get; set; }
    public string? Pid { get; set; }
}

public sealed class CANaerospaceIdentifierAssignment
{
    public string Name { get; set; } = "Project Local Joystick Assignment";
    public string Range { get; set; } = "1500-1799";
    public string Notes { get; set; } = "User-defined CANaerospace normal-operation identifiers for joystick translator.";
}

public sealed class CANaerospaceServiceCodePolicy
{
    public string Valid { get; set; } = "0x00";
    public string DataStale { get; set; } = "0x01";
    public string UsbDisconnected { get; set; } = "0x02";
    public string HidParseError { get; set; } = "0x04";
    public string TxBackpressureSeen { get; set; } = "0x08";
    public string JoystickNotPresent { get; set; } = "0x10";
    public string MappingError { get; set; } = "0x20";
}

public sealed class CANaerospaceParameter
{
    public bool Enabled { get; set; } = true;
    public string Name { get; set; } = "";
    public int CanIdDecimal { get; set; }
    public string CanIdHex => $"0x{CanIdDecimal:X3}";
    public string DataType { get; set; } = "USHORT2";
    public string DataTypeCode => $"0x{CANaerospaceDataType.FromName(DataType).Code:X2}";
    public List<string> Sources { get; set; } = [];
    public Dictionary<string, string> ByteLayout { get; set; } = new();
    public string? BitPacking { get; set; }
    public bool ActiveHigh { get; set; } = true;

    public static List<CANaerospaceParameter> CreateDefaults() =>
    [
        new() { Name = "Joystick Axis X/Y", CanIdDecimal = 1500, DataType = "USHORT2", Sources = ["X", "Y"], ByteLayout = new() { ["byte4"] = "X_msb", ["byte5"] = "X_lsb", ["byte6"] = "Y_msb", ["byte7"] = "Y_lsb" } },
        new() { Name = "Joystick Axis Z/Rx", CanIdDecimal = 1501, DataType = "USHORT2", Sources = ["Z", "Rx"], ByteLayout = new() { ["byte4"] = "Z_msb", ["byte5"] = "Z_lsb", ["byte6"] = "Rx_msb", ["byte7"] = "Rx_lsb" } },
        new() { Name = "Joystick Axis Ry/Rz", CanIdDecimal = 1502, DataType = "USHORT2", Sources = ["Ry", "Rz"], ByteLayout = new() { ["byte4"] = "Ry_msb", ["byte5"] = "Ry_lsb", ["byte6"] = "Rz_msb", ["byte7"] = "Rz_lsb" } },
        new() { Name = "Joystick Axis Slider/Dial", CanIdDecimal = 1503, DataType = "USHORT2", Sources = ["Slider", "Dial"], ByteLayout = new() { ["byte4"] = "Slider_msb", ["byte5"] = "Slider_lsb", ["byte6"] = "Dial_msb", ["byte7"] = "Dial_lsb" } },
        new() { Name = "Joystick Buttons 0..31", CanIdDecimal = 1504, DataType = "BLONG", Sources = [.. Enumerable.Range(0, 32).Select(i => $"Button{i}")], BitPacking = "big-endian-can-aerospace" },
        new() { Name = "Joystick Buttons 32..63", CanIdDecimal = 1505, DataType = "BLONG", Sources = [.. Enumerable.Range(32, 32).Select(i => $"Button{i}")], BitPacking = "big-endian-can-aerospace" },
        new() { Name = "Joystick Status", CanIdDecimal = 1506, DataType = "UCHAR4", Sources = ["StatusFlags", "HidSampleCounter", "CanFrameCounter", "LastReportAgeBucket"], ByteLayout = new() { ["byte4"] = "status_flags", ["byte5"] = "hid_sample_counter", ["byte6"] = "can_frame_counter", ["byte7"] = "last_report_age_bucket" } },
    ];
}
