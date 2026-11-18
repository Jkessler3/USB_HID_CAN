namespace CANaerospaceJoystickMapper.Core.Models;

// A version-1 profile carries frame definitions and editable signal rows. A row
// left at its factory ID follows its named frame when that frame is reassigned.
// An explicitly reassigned row can also route one signal to a different CAN ID.
internal sealed class PreviewLayout
{
    private static readonly Dictionary<string, CANaerospaceParameter> FactoryFrames = CANaerospaceParameter.CreateDefaults()
        .ToDictionary(p => p.Name, StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, CANaerospaceParameter> parameters = new(StringComparer.OrdinalIgnoreCase);
    private readonly SortedDictionary<int, PreviewFrame> frames = new();
    public List<string> Errors { get; } = [];
    public IEnumerable<PreviewFrame> Frames => frames.Values;

    public PreviewLayout(MapperProfile profile)
    {
        if (profile.Axes == null || profile.Buttons == null || profile.Parameters == null)
        {
            Errors.Add("The profile needs axis, button and parameter lists.");
            return;
        }
        foreach (var p in profile.Parameters)
        {
            if (p == null || string.IsNullOrWhiteSpace(p.Name))
            {
                Errors.Add("Every frame definition needs a name.");
                continue;
            }
            if (!parameters.TryAdd(p.Name, p))
                Errors.Add($"Duplicate frame name: {p.Name}.");
            if (!p.Enabled) continue;
            if (p.CanIdDecimal is < 1 or > 0x7ff)
                Errors.Add($"Invalid standard CAN ID for {p.Name}.");
            var type = CANaerospaceDataType.FromName(p.DataType);
            if (FactoryFrames.TryGetValue(p.Name, out var factory) && type != CANaerospaceDataType.FromName(factory.DataType))
                Errors.Add($"Frame {p.Name} requires {factory.DataType}.");
            if (type == CANaerospaceDataType.UChar4 && (p.Name.Equals("Joystick Status", StringComparison.OrdinalIgnoreCase)
                || p.Sources?.Contains("StatusFlags", StringComparer.OrdinalIgnoreCase) == true))
            {
                if (frames.ContainsKey(p.CanIdDecimal))
                    Errors.Add($"Duplicate status CAN ID {p.CanIdDecimal}.");
                else
                    frames.Add(p.CanIdDecimal, new PreviewFrame(p.CanIdDecimal, p.Name, type));
            }
            else if (type != CANaerospaceDataType.UShort2 && type != CANaerospaceDataType.BLong)
                Errors.Add($"Unsupported frame type for {p.Name}.");
        }
        foreach (var axis in profile.Axes)
        {
            if (axis == null) { Errors.Add("Empty axis mapping."); continue; }
            if (!axis.Enabled) continue;
            var errors = axis.Validate();
            if (errors.Count > 0) { Errors.AddRange(errors.Select(e => $"{axis.Source}: {e}")); continue; }
            var frame = GetTarget(axis.ParameterName, axis.CanIdDecimal, CANaerospaceDataType.UShort2);
            if (frame == null) continue;
            if (frame.Axes.Any(a => a.DataBytePosition == axis.DataBytePosition))
                Errors.Add($"CAN ID {frame.Id} maps two axes to byte {axis.DataBytePosition}.");
            frame.Axes.Add(axis);
        }
        foreach (var button in profile.Buttons)
        {
            if (button == null) { Errors.Add("Empty button mapping."); continue; }
            if (!button.Enabled) continue;
            var errors = button.Validate();
            if (errors.Count > 0) { Errors.AddRange(errors.Select(e => $"Button {button.ButtonIndex}: {e}")); continue; }
            var frame = GetTarget(button.ParameterName, button.CanIdDecimal, CANaerospaceDataType.BLong);
            if (frame == null) continue;
            if (frame.Buttons.Any(b => b.BitIndex == button.BitIndex))
                Errors.Add($"CAN ID {frame.Id} maps two buttons to bit {button.BitIndex}.");
            frame.Buttons.Add(button);
        }
    }

    private PreviewFrame? GetTarget(string name, int id, CANaerospaceDataType type)
    {
        if (parameters.TryGetValue(name, out var p))
        {
            if (!p.Enabled) return null;
            if (CANaerospaceDataType.FromName(p.DataType) != type)
            {
                Errors.Add($"The signal type does not match frame {name}.");
                return null;
            }
            if (FactoryFrames.TryGetValue(name, out var factory) && id == factory.CanIdDecimal)
                id = p.CanIdDecimal;
        }
        if (!frames.TryGetValue(id, out var frame))
            frames.Add(id, frame = new PreviewFrame(id, name, type));
        else if (frame.Type != type)
        {
            Errors.Add($"CAN ID {id} has incompatible data types.");
            return null;
        }
        return frame;
    }
}

internal sealed record PreviewFrame(int Id, string Name, CANaerospaceDataType Type)
{
    public List<AxisMapping> Axes { get; } = [];
    public List<ButtonMapping> Buttons { get; } = [];
}
