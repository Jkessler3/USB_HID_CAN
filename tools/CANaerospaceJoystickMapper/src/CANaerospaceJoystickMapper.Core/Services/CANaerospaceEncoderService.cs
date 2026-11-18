using CANaerospaceJoystickMapper.Core.Models;

namespace CANaerospaceJoystickMapper.Core.Services;

public sealed class CANaerospaceEncoderService
{
    private readonly Dictionary<int, byte> _messageCodes = new();

    public IReadOnlyList<CANaerospaceFrame> EncodeJoystickSample(MapperProfile profile, JoystickSample sample)
    {
        var layout = new PreviewLayout(profile);
        if (profile.NodeId == 0 || layout.Errors.Count != 0)
            throw new ArgumentException("Invalid preview profile. " + string.Join(" ", layout.Errors), nameof(profile));
        return EncodeJoystickSample(layout, profile.NodeId, sample);
    }

    internal IReadOnlyList<CANaerospaceFrame> EncodeJoystickSample(PreviewLayout layout, byte nodeId, JoystickSample sample)
    {
        var frames = new List<CANaerospaceFrame>();
        bool usable = sample.HidSampleValid && sample.JoystickPresent && (sample.StatusFlags & CANaerospaceServiceCode.MappingError) == 0;
        foreach (var target in layout.Frames)
        {
            byte[] data;
            Dictionary<string, object> values;
            if (target.Type == CANaerospaceDataType.UShort2)
            {
                data = [0x80, 0, 0x80, 0];
                foreach (var axis in target.Axes)
                {
                    if (!usable || !sample.Axes.TryGetValue(axis.Source, out var input)) continue;
                    int value = axis.Apply(input.Raw);
                    int pos = axis.DataBytePosition - 4;
                    data[pos] = (byte)(value >> 8);
                    data[pos + 1] = (byte)value;
                }
                values = new() { ["A"] = (ushort)((data[0] << 8) | data[1]), ["B"] = (ushort)((data[2] << 8) | data[3]) };
            }
            else if (target.Type == CANaerospaceDataType.BLong)
            {
                uint bits = 0;
                foreach (var button in target.Buttons)
                    if (usable && button.ButtonIndex < sample.Buttons.Length && sample.Buttons[button.ButtonIndex] == button.ActiveHigh)
                        bits |= button.Mask;
                data = [(byte)(bits >> 24), (byte)(bits >> 16), (byte)(bits >> 8), (byte)bits];
                values = new() { ["Buttons"] = $"0x{bits:X8}" };
            }
            else
            {
                byte age = (byte)Math.Clamp((DateTimeOffset.Now - sample.UpdatedAt).TotalMilliseconds / 10.0, 0, 255);
                data = [sample.StatusFlags, sample.SampleCounter, sample.CanFrameCounter, age];
                values = new() { ["statusFlags"] = $"0x{data[0]:X2}", ["hidSampleCounter"] = data[1], ["canFrameCounter"] = data[2], ["ageBucket"] = age };
            }
            frames.Add(new CANaerospaceFrame
            {
                CanIdDecimal = target.Id, ParameterName = target.Name, NodeId = nodeId, DataType = target.Type,
                ServiceCode = sample.StatusFlags, MessageCode = NextMessageCode(target.Id), Data = data, Values = values,
                Summary = $"{target.Id}/0x{target.Id:X3} {target.Name}: {Convert.ToHexString(data)}"
            });
        }
        return frames;
    }

    public byte NextMessageCode(int canId)
    {
        _messageCodes.TryGetValue(canId, out var current);
        _messageCodes[canId] = unchecked((byte)(current + 1));
        return current;
    }
}
