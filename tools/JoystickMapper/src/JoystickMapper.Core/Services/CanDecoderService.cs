using System.Globalization;
using System.Text.RegularExpressions;
using JoystickMapper.Core.Models;

namespace JoystickMapper.Core.Services;

public sealed class CanDecoderService
{
    private static readonly Regex CompactRegex = new(@"\b(?<id>[0-9a-fA-F]{1,8})#(?<data>[0-9a-fA-F ]+)\b", RegexOptions.Compiled);
    private static readonly Regex SpacedRegex = new(@"\b(?<id>0x[0-9a-fA-F]+|[0-9a-fA-F]{3})\b\s+(?:\[(?<dlc>\d+)\]\s+)?(?<data>(?:[0-9a-fA-F]{2}\s*){1,64})", RegexOptions.Compiled);

    public DecodedCanFrame DecodeLine(string line)
    {
        try
        {
            var parsed = TryParseLine(line, out var canId, out var payload, out var error);
            if (!parsed)
                return Invalid(0, [], error ?? "Unable to parse CAN line.");
            return Decode(canId, payload);
        }
        catch (Exception ex)
        {
            return Invalid(0, [], $"Malformed input: {ex.Message}");
        }
    }

    public DecodedCanFrame Decode(int canId, byte[] payload)
    {
        if (payload.Length != 8)
            return Invalid(canId, payload, $"Expected DLC 8, got {payload.Length}.");

        return canId switch
        {
            Can301ControlFrame.Id => Decode301(payload),
            Can302StatusFrame.Id => Decode302(payload),
            _ => Invalid(canId, payload, $"Unsupported CAN ID 0x{canId:X}.")
        };
    }

    private static DecodedCanFrame Decode301(byte[] payload)
    {
        var frame = new Can301ControlFrame
        {
            Steer = ReadInt16Le(payload, 0),
            Throttle = ReadInt16Le(payload, 2),
            Aux0 = unchecked((sbyte)payload[4]),
            Aux1 = unchecked((sbyte)payload[5]),
            Buttons = payload[6],
            SampleCounter = payload[7]
        };

        return new DecodedCanFrame
        {
            CanId = Can301ControlFrame.Id,
            Payload = payload,
            FrameType = "0x301 Control",
            IsValid = true,
            Control = frame,
            Summary = $"Control sample={frame.SampleCounter} steer={frame.Steer} throttle={frame.Throttle} buttons=0x{frame.Buttons:X2}"
        };
    }

    private static DecodedCanFrame Decode302(byte[] payload)
    {
        var frame = new Can302StatusFrame
        {
            StatusFlags = payload[0],
            AxisCount = payload[1],
            SampleCounter = payload[2],
            FrameCounter = payload[3],
            LastReportMs = BitConverter.ToUInt32(payload, 4)
        };
        var flags = frame.FlagNames.Count == 0 ? "none" : string.Join(", ", frame.FlagNames);

        return new DecodedCanFrame
        {
            CanId = Can302StatusFrame.Id,
            Payload = payload,
            FrameType = "0x302 Status",
            IsValid = true,
            Status = frame,
            Summary = $"Status sample={frame.SampleCounter} frame={frame.FrameCounter} axes={frame.AxisCount} flags=[{flags}] lastReportMs={frame.LastReportMs}"
        };
    }

    private static bool TryParseLine(string line, out int canId, out byte[] payload, out string? error)
    {
        canId = 0;
        payload = [];
        error = null;

        if (string.IsNullOrWhiteSpace(line))
        {
            error = "Line is empty.";
            return false;
        }

        var match = CompactRegex.Match(line);
        if (!match.Success)
            match = SpacedRegex.Match(line);
        if (!match.Success)
        {
            error = "No CAN ID and payload found.";
            return false;
        }

        var idText = match.Groups["id"].Value;
        if (idText.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            idText = idText[2..];
        if (!int.TryParse(idText, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out canId))
        {
            error = "CAN ID is not valid hex.";
            return false;
        }

        var dataText = Regex.Replace(match.Groups["data"].Value, @"\s+", "");
        if ((dataText.Length % 2) != 0)
        {
            error = "Payload must contain whole bytes.";
            return false;
        }

        try
        {
            payload = Convert.FromHexString(dataText);
            return true;
        }
        catch (FormatException)
        {
            error = "Payload contains non-hex characters.";
            return false;
        }
    }

    private static short ReadInt16Le(byte[] data, int offset) => unchecked((short)(data[offset] | (data[offset + 1] << 8)));

    private static DecodedCanFrame Invalid(int canId, byte[] payload, string error) => new()
    {
        CanId = canId,
        Payload = payload,
        FrameType = "Invalid",
        IsValid = false,
        Error = error,
        Summary = error
    };
}
