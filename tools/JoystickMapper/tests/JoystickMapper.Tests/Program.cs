using JoystickMapper.Core.Models;
using JoystickMapper.Core.Services;

var tests = new List<(string Name, Action Test)>
{
    ("Default profile validates", DefaultProfileValidates),
    ("Profile JSON round trip", ProfileJsonRoundTrip),
    ("Axis scaling deadzone inversion", AxisScalingDeadzoneInversion),
    ("CAN 0x301 valid parse", Can301ValidParse),
    ("CAN 0x302 flags decode", Can302FlagsDecode),
    ("Malformed CAN line does not crash", MalformedCanLineDoesNotCrash),
};

var failures = 0;
foreach (var (name, test) in tests)
{
    try
    {
        test();
        Console.WriteLine($"PASS {name}");
    }
    catch (Exception ex)
    {
        failures++;
        Console.WriteLine($"FAIL {name}: {ex.Message}");
    }
}

return failures == 0 ? 0 : 1;

static void DefaultProfileValidates()
{
    var errors = MapperProfile.CreateDefault().Validate();
    Assert(errors.Count == 0, string.Join("; ", errors));
}

static void ProfileJsonRoundTrip()
{
    var service = new ProfileService();
    var profile = MapperProfile.CreateDefault();
    profile.ProfileName = "Round Trip";
    var restored = service.Deserialize(service.Serialize(profile));
    Assert(restored.ProfileName == "Round Trip", "Profile name did not round-trip.");
    Assert(restored.Axes.Count == profile.Axes.Count, "Axis count did not round-trip.");
}

static void AxisScalingDeadzoneInversion()
{
    var axis = new AxisMapping { Deadzone = 100, InputMin = -1000, InputMax = 1000, OutputMin = -1000, OutputMax = 1000 };
    Assert(axis.Apply(50) == 0, "Deadzone did not force zero.");
    Assert(axis.Apply(1000) == 1000, "Max scale failed.");
    axis.Invert = true;
    Assert(axis.Apply(1000) == -1000, "Invert failed.");
}

static void Can301ValidParse()
{
    var decoder = new CanDecoderService();
    var decoded = decoder.DecodeLine("301#0000100000000142");
    Assert(decoded.IsValid, decoded.Error ?? "Invalid.");
    Assert(decoded.Control != null, "Control frame missing.");
    Assert(decoded.Control.Throttle == 16, "Throttle decode failed.");
    Assert(decoded.Control.Buttons == 1, "Button decode failed.");
    Assert(decoded.Control.SampleCounter == 0x42, "Sample counter decode failed.");
}

static void Can302FlagsDecode()
{
    var decoder = new CanDecoderService();
    var decoded = decoder.DecodeLine("302#0304421001020304");
    Assert(decoded.IsValid, decoded.Error ?? "Invalid.");
    Assert(decoded.Status != null, "Status frame missing.");
    Assert((decoded.Status.StatusFlags & Can302StatusFrame.JoystickPresent) != 0, "Joystick present flag missing.");
    Assert((decoded.Status.StatusFlags & Can302StatusFrame.HidSampleValid) != 0, "HID valid flag missing.");
    Assert(decoded.Status.SampleCounter == 0x42, "Sample counter decode failed.");
}

static void MalformedCanLineDoesNotCrash()
{
    var decoder = new CanDecoderService();
    var decoded = decoder.DecodeLine("not a can frame");
    Assert(!decoded.IsValid, "Malformed line should be invalid.");
}

static void Assert(bool condition, string message)
{
    if (!condition)
        throw new InvalidOperationException(message);
}
