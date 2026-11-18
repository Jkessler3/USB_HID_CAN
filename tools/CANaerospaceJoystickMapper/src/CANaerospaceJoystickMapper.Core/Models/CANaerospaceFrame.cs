namespace CANaerospaceJoystickMapper.Core.Models;

public sealed class CANaerospaceFrame
{
    public int CanIdDecimal { get; init; }
    public string CanIdHex => $"0x{CanIdDecimal:X3}";
    public byte NodeId { get; init; }
    public CANaerospaceDataType DataType { get; init; } = CANaerospaceDataType.Unknown;
    public byte ServiceCode { get; init; }
    public byte MessageCode { get; init; }
    public byte[] Data { get; init; } = new byte[4];
    public byte[] Payload => [NodeId, DataType.Code, ServiceCode, MessageCode, Data[0], Data[1], Data[2], Data[3]];
    public string ParameterName { get; init; } = "";
    public string Summary { get; init; } = "";
    public bool IsValid { get; init; } = true;
    public string? Error { get; init; }
    public IReadOnlyList<string> Warnings { get; init; } = [];
    public IReadOnlyDictionary<string, object> Values { get; init; } = new Dictionary<string, object>();
}

public sealed class DecodedCanFrame
{
    public int CanIdDecimal { get; init; }
    public string CanIdHex => CanIdDecimal > 0 ? $"0x{CanIdDecimal:X3}" : "n/a";
    public byte[] Payload { get; init; } = [];
    public string FrameType { get; init; } = "CANaerospace";
    public string Summary { get; init; } = "";
    public bool IsValid { get; init; }
    public string? Error { get; init; }
    public CANaerospaceFrame? Frame { get; init; }
}

public sealed class CANaerospaceDataType
{
    public string Name { get; }
    public byte Code { get; }

    private CANaerospaceDataType(string name, byte code)
    {
        Name = name;
        Code = code;
    }

    public static readonly CANaerospaceDataType Unknown = new("UNKNOWN", 0xFF);
    public static readonly CANaerospaceDataType Long = new("LONG", 0x03);
    public static readonly CANaerospaceDataType ULong = new("ULONG", 0x04);
    public static readonly CANaerospaceDataType BLong = new("BLONG", 0x05);
    public static readonly CANaerospaceDataType Short = new("SHORT", 0x06);
    public static readonly CANaerospaceDataType UShort = new("USHORT", 0x07);
    public static readonly CANaerospaceDataType BShort = new("BSHORT", 0x08);
    public static readonly CANaerospaceDataType UChar = new("UCHAR", 0x0A);
    public static readonly CANaerospaceDataType BChar = new("BCHAR", 0x0B);
    public static readonly CANaerospaceDataType Short2 = new("SHORT2", 0x0C);
    public static readonly CANaerospaceDataType UShort2 = new("USHORT2", 0x0D);
    public static readonly CANaerospaceDataType BShort2 = new("BSHORT2", 0x0E);
    public static readonly CANaerospaceDataType UChar4 = new("UCHAR4", 0x10);
    public static readonly CANaerospaceDataType BChar4 = new("BCHAR4", 0x11);
    public static readonly CANaerospaceDataType UChar2 = new("UCHAR2", 0x13);
    public static readonly CANaerospaceDataType BChar2 = new("BCHAR2", 0x14);

    public static IReadOnlyList<CANaerospaceDataType> All { get; } =
    [
        Long, ULong, BLong, Short, UShort, BShort, UChar, BChar,
        Short2, UShort2, BShort2, UChar4, BChar4, UChar2, BChar2
    ];

    public static CANaerospaceDataType FromCode(byte code) =>
        All.FirstOrDefault(x => x.Code == code) ?? Unknown;

    public static CANaerospaceDataType FromName(string? name) =>
        All.FirstOrDefault(x => string.Equals(x.Name, name, StringComparison.OrdinalIgnoreCase)) ?? Unknown;
}

public static class CANaerospaceServiceCode
{
    public const byte Valid = 0x00;
    public const byte DataStale = 0x01;
    public const byte UsbDisconnected = 0x02;
    public const byte HidParseError = 0x04;
    public const byte TxBackpressureSeen = 0x08;
    public const byte JoystickNotPresent = 0x10;
    public const byte MappingError = 0x20;

    public static IReadOnlyList<string> Decode(byte code)
    {
        var names = new List<string>();
        if (code == 0) names.Add("valid / normal");
        if ((code & DataStale) != 0) names.Add("data stale");
        if ((code & UsbDisconnected) != 0) names.Add("USB disconnected");
        if ((code & HidParseError) != 0) names.Add("HID parse error");
        if ((code & TxBackpressureSeen) != 0) names.Add("CAN TX backpressure seen");
        if ((code & JoystickNotPresent) != 0) names.Add("joystick not present");
        if ((code & MappingError) != 0) names.Add("mapping/profile error");
        if ((code & 0xC0) != 0) names.Add("reserved bit set");
        return names;
    }
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
