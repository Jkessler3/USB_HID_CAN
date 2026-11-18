using System.Globalization;
using System.Text.RegularExpressions;
using CANaerospaceJoystickMapper.Core.Models;

namespace CANaerospaceJoystickMapper.Core.Services;

public sealed class CANaerospaceDecoderService
{
    private static readonly Regex FrameLine = new(
        @"\A[ \t]*(?:\([0-9]+(?:\.[0-9]+)?\)[ \t]+)?(?:(?<interface>[A-Za-z][A-Za-z0-9_.:-]*)[ \t]+)?" +
        @"(?<id>0[xX][0-9A-Fa-f]{1,3}|[0-9A-Fa-f]{1,4})(?:#[ \t]*|[ \t]+(?:\[(?<dlc>[0-9]+)\][ \t]+)?)" +
        @"(?<data>[0-9A-Fa-f]+(?:[ \t]+[0-9A-Fa-f]+)*)[ \t]*\z",
        RegexOptions.Compiled | RegexOptions.NonBacktracking);
    private readonly Dictionary<(byte Node, int Id), byte> _lastMessageCodes = new();
    private readonly Dictionary<int, Parameter> _parameters;
    private readonly byte _nodeId;

    private sealed record Parameter(string Name, CANaerospaceDataType Type, string FirstAxis, string SecondAxis);

    public CANaerospaceDecoderService() : this(MapperProfile.CreateDefault()) { }

    public CANaerospaceDecoderService(MapperProfile profile)
    {
        _nodeId = profile.NodeId;
        var layout = new PreviewLayout(profile);
        if (layout.Errors.Count > 0)
            throw new ArgumentException(string.Join(" ", layout.Errors), nameof(profile));
        _parameters = layout.Frames.ToDictionary(f => f.Id, f => new Parameter(
            f.Name, f.Type, f.Axes.FirstOrDefault(a => a.DataBytePosition == 4)?.Source ?? "A",
            f.Axes.FirstOrDefault(a => a.DataBytePosition == 6)?.Source ?? "B"));
    }

    public void ResetSequence() => _lastMessageCodes.Clear();

    public DecodedCanFrame DecodeLine(string line) => DecodeLine(line, _nodeId);

    public DecodedCanFrame DecodeLine(string line, byte expectedNodeId)
    {
        if (!TryParseLine(line, out var canId, out var payload, out var error))
            return Invalid(canId, payload, error);
        return Decode(canId, payload, expectedNodeId);
    }

    public DecodedCanFrame Decode(int canId, byte[] payload) => Decode(canId, payload, _nodeId);

    public DecodedCanFrame Decode(int canId, byte[] payload, byte expectedNodeId)
    {
        if (canId is < 0 or > 0x7FF)
        {
            return Invalid(canId, payload, "Only standard 11-bit CAN identifiers are supported.");
        }
        if (payload.Length != 8)
            return Invalid(canId, payload, $"Invalid DLC: CANaerospace frames must be exactly 8 bytes, got {payload.Length}.");

        var warnings = new List<string>();
        var type = CANaerospaceDataType.FromCode(payload[1]);
        _parameters.TryGetValue(canId, out var parameter);
        if (parameter != null && parameter.Type.Code != payload[1])
            return Invalid(canId, payload, $"CAN ID {canId}/0x{canId:X3} expected data type {parameter.Type.Name}/0x{parameter.Type.Code:X2}, got 0x{payload[1]:X2}.");
        if (parameter == null)
            warnings.Add("CAN ID is unassigned in this profile.");
        if (type == CANaerospaceDataType.Unknown)
            warnings.Add($"Unknown data type 0x{payload[1]:X2}.");
        if (payload[0] != expectedNodeId)
            warnings.Add($"Unexpected Node-ID {payload[0]}; expected {expectedNodeId}.");
        if (payload[2] != 0)
            warnings.Add($"Service Code fault bits: {string.Join(", ", CANaerospaceServiceCode.Decode(payload[2]))}.");
        var stream = (payload[0], canId);
        if (_lastMessageCodes.TryGetValue(stream, out var previous) && payload[3] != unchecked((byte)(previous + 1)))
            warnings.Add($"Message Code gap for CAN ID {canId}/0x{canId:X3}: previous 0x{previous:X2}, current 0x{payload[3]:X2}.");
        _lastMessageCodes[stream] = payload[3];

        var data = payload[4..8];
        var values = DecodeValues(parameter, type, data);
        var name = parameter?.Name ?? "Unassigned / external CANaerospace parameter";
        var frame = new CANaerospaceFrame
        {
            CanIdDecimal = canId,
            NodeId = payload[0],
            DataType = type,
            ServiceCode = payload[2],
            MessageCode = payload[3],
            Data = data,
            ParameterName = name,
            Values = values,
            Warnings = warnings,
            Summary = $"{canId}/0x{canId:X3} {name}: node={payload[0]} type={type.Name}/0x{payload[1]:X2} svc=0x{payload[2]:X2} msg=0x{payload[3]:X2} {FormatValues(values)}"
        };

        return new DecodedCanFrame
        {
            CanIdDecimal = canId,
            Payload = payload,
            FrameType = "CANaerospace",
            IsValid = true,
            Frame = frame,
            Summary = warnings.Count == 0 ? frame.Summary : $"{frame.Summary} WARN: {string.Join(" | ", warnings)}"
        };
    }

    private static IReadOnlyDictionary<string, object> DecodeValues(Parameter? parameter, CANaerospaceDataType type, byte[] data)
    {
        var values = new Dictionary<string, object>();
        switch (type.Code)
        {
            case 0x03: values["LONG"] = ReadI32Be(data); break;
            case 0x04: values["ULONG"] = ReadU32Be(data); break;
            case 0x05: values["BLONG"] = $"0x{ReadU32Be(data):X8}"; break;
            case 0x06: values["SHORT"] = ReadI16Be(data, 0); break;
            case 0x07: values["USHORT"] = ReadU16Be(data, 0); break;
            case 0x08: values["BSHORT"] = $"0x{ReadU16Be(data, 0):X4}"; break;
            case 0x0A: values["UCHAR"] = data[0]; break;
            case 0x0B: values["BCHAR"] = $"0x{data[0]:X2}"; break;
            case 0x0C: values["A"] = ReadI16Be(data, 0); values["B"] = ReadI16Be(data, 2); break;
            case 0x0D:
                values[parameter?.FirstAxis ?? "A"] = ReadU16Be(data, 0);
                values[parameter?.SecondAxis ?? "B"] = ReadU16Be(data, 2);
                break;
            case 0x0E: values["A"] = $"0x{ReadU16Be(data, 0):X4}"; values["B"] = $"0x{ReadU16Be(data, 2):X4}"; break;
            case 0x10: values["B4"] = data[0]; values["B5"] = data[1]; values["B6"] = data[2]; values["B7"] = data[3]; break;
            case 0x11: values["Flags"] = $"0x{data[0]:X2} 0x{data[1]:X2} 0x{data[2]:X2} 0x{data[3]:X2}"; break;
            case 0x13: values["A"] = data[0]; values["B"] = data[1]; break;
            case 0x14: values["A"] = $"0x{data[0]:X2}"; values["B"] = $"0x{data[1]:X2}"; break;
        }
        if (parameter?.Type == CANaerospaceDataType.UChar4)
        {
            values["statusFlags"] = $"0x{data[0]:X2}";
            values["hidSampleCounter"] = data[1];
            values["canFrameCounter"] = data[2];
            values["ageBucket"] = data[3];
        }
        return values;
    }

    private static bool TryParseLine(string line, out int canId, out byte[] payload, out string error)
    {
        canId = 0;
        payload = [];
        error = "";
        if (string.IsNullOrWhiteSpace(line))
        {
            error = "Line is empty.";
            return false;
        }

        var match = FrameLine.Match(line);
        if (!match.Success)
        {
            error = "Expected one complete standard CAN data frame, with a CAN ID and hexadecimal payload.";
            return false;
        }

        var idText = match.Groups["id"].Value;
        bool prefixed = idText.StartsWith("0x", StringComparison.OrdinalIgnoreCase);
        bool hasInterface = match.Groups["interface"].Success;
        bool candumpHex = hasInterface && idText.Length == 3;
        // Preserve the documented "can0 1500#" decimal spelling. A leading
        // zero in a four-digit field is not a decimal or standard candump ID.
        bool projectDecimal = idText.Length == 4 && idText[0] != '0' && idText.All(char.IsAsciiDigit);
        if (hasInterface && !prefixed && !candumpHex && !projectDecimal)
        {
            error = "Expected a three-digit standard SocketCAN identifier or a project decimal ID.";
            return false;
        }
        bool useHex = prefixed || candumpHex || idText.Any(char.IsAsciiLetter);
        if ((!prefixed && useHex && idText.Length > 3) ||
            !int.TryParse(prefixed ? idText.AsSpan(2) : idText.AsSpan(),
                          useHex ? NumberStyles.AllowHexSpecifier : NumberStyles.None,
                          CultureInfo.InvariantCulture, out canId) || canId > 0x7FF)
        {
            error = "CAN ID must identify a standard 11-bit frame.";
            return false;
        }

        var dlc = match.Groups["dlc"];
        if (dlc.Success && (!int.TryParse(dlc.Value, NumberStyles.None, CultureInfo.InvariantCulture, out int length) || length != 8))
        {
            error = $"Invalid declared DLC [{dlc.Value}]; expected 8.";
            return false;
        }

        string[] groups = match.Groups["data"].Value.Split([' ', '\t'], StringSplitOptions.RemoveEmptyEntries);
        if (groups.Any(group => group.Length % 2 != 0) || groups.Sum(group => group.Length) != 16)
        {
            error = "Invalid payload: expected exactly eight whole hexadecimal bytes.";
            return false;
        }
        payload = Convert.FromHexString(string.Concat(groups));
        return true;
    }

    private static string FormatValues(IReadOnlyDictionary<string, object> values) =>
        values.Count == 0 ? "data=raw" : string.Join(" ", values.Select(x => $"{x.Key}={x.Value}"));

    private static ushort ReadU16Be(byte[] data, int offset) => (ushort)((data[offset] << 8) | data[offset + 1]);
    private static short ReadI16Be(byte[] data, int offset) => unchecked((short)ReadU16Be(data, offset));
    private static uint ReadU32Be(byte[] data) => ((uint)data[0] << 24) | ((uint)data[1] << 16) | ((uint)data[2] << 8) | data[3];
    private static int ReadI32Be(byte[] data) => unchecked((int)ReadU32Be(data));

    private static DecodedCanFrame Invalid(int canId, byte[] payload, string error) => new()
    {
        CanIdDecimal = canId,
        Payload = payload,
        FrameType = "Invalid",
        IsValid = false,
        Error = error,
        Summary = error
    };
}
