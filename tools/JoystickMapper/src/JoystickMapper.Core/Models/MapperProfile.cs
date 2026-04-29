using System.Text.Json.Serialization;

namespace JoystickMapper.Core.Models;

public sealed class MapperProfile
{
    public int Version { get; set; } = 1;
    public string ProfileName { get; set; } = "Default Joystick Mapping";
    public DeviceInfo Device { get; set; } = new();
    public CanSettings Can { get; set; } = new();
    public List<AxisMapping> Axes { get; set; } = AxisMapping.CreateDefaults();
    public List<ButtonMapping> Buttons { get; set; } = ButtonMapping.CreateDefaults();

    [JsonIgnore]
    public string Summary => $"{ProfileName} ({Axes.Count(a => a.Enabled)} axes, {Buttons.Count(b => b.Enabled)} buttons)";

    public static MapperProfile CreateDefault() => new();

    public IReadOnlyList<string> Validate()
    {
        var errors = new List<string>();
        if (Version <= 0)
            errors.Add("Profile version must be positive.");
        if (string.IsNullOrWhiteSpace(ProfileName))
            errors.Add("Profile name is required.");
        if (!CanId.TryParse(Can.ControlFrameId, out _))
            errors.Add("Control CAN ID must be a valid hex or decimal identifier.");
        if (!CanId.TryParse(Can.StatusFrameId, out _))
            errors.Add("Status CAN ID must be a valid hex or decimal identifier.");

        foreach (var axis in Axes.Select((value, index) => (value, index)))
        {
            foreach (var error in axis.value.Validate())
                errors.Add($"Axis row {axis.index + 1}: {error}");
        }

        foreach (var button in Buttons.Select((value, index) => (value, index)))
        {
            foreach (var error in button.value.Validate())
                errors.Add($"Button row {button.index + 1}: {error}");
        }

        return errors;
    }
}

public sealed class DeviceInfo
{
    public string Name { get; set; } = "Generic HID Joystick";
    public string? Vid { get; set; }
    public string? Pid { get; set; }
}

public sealed class CanSettings
{
    public string ControlFrameId { get; set; } = "0x301";
    public string StatusFrameId { get; set; } = "0x302";
    public int ReceiverTimeoutMs { get; set; } = 250;
}
