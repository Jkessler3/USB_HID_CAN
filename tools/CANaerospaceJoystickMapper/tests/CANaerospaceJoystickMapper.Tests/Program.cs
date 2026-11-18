using CANaerospaceJoystickMapper.Core.Models;
using CANaerospaceJoystickMapper.Core.Services;

var tests = new List<(string Name, Action Test)>
{
    ("USB diagnostic extended and sparse reports", PreviewTests.ReportCounts),
    ("Offline diagnostics and capture parsing", DiagnosticsTests.Run),
    ("Invalid profile save preserves existing JSON", PreviewTests.ProtectSavedProfile),
    ("Inverted range and scale", PreviewTests.InvertedRangeAndScale),
    ("Applied profile routing and failsafe", PreviewTests.ProfileRouting),
    ("Incompatible profile edits rejected", PreviewTests.InvalidEdits),
    ("Applied view and manual replay", PreviewTests.AppliedViewAndManualReplay),
    ("Default profile validates", DefaultProfileValidates),
    ("Profile JSON round trip", ProfileJsonRoundTrip),
    ("Axis scaling deadzone inversion", AxisScalingDeadzoneInversion),
    ("CANaerospace test vector decodes", CANaerospaceTestVectorDecodes),
    ("Big-endian USHORT2 encodes", BigEndianUShort2Encodes),
    ("BLONG button bit packing encodes", BLongButtonPackingEncodes),
    ("Service Code flags decode", ServiceCodeFlagsDecode),
    ("Message Code rolls over", MessageCodeRollsOver),
    ("Decimal and hex CAN IDs display", DecimalAndHexCanIdsDisplay),
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
    Assert(restored.Protocol == "CANaerospace", "Protocol did not round-trip.");
    Assert(restored.Parameters.Count == profile.Parameters.Count, "Parameter count did not round-trip.");
}

static void AxisScalingDeadzoneInversion()
{
    var axis = new AxisMapping { Deadzone = 100, InputMin = -1000, InputMax = 1000, OutputMin = 0, OutputMax = 65535 };
    Assert(axis.Apply(50) == 32768, "Deadzone did not center the axis.");
    axis.Invert = true;
    Assert(axis.Apply(1000) == 0, "Invert failed.");
}

static void CANaerospaceTestVectorDecodes()
{
    var decoded = new CANaerospaceDecoderService().DecodeLine("1500#010D002280007FFF");
    Assert(decoded.IsValid, decoded.Error ?? "Invalid.");
    Assert(decoded.Frame != null, "Frame missing.");
    var frame = decoded.Frame!;
    Assert(frame.NodeId == 1, "Node-ID failed.");
    Assert(frame.DataType.Name == "USHORT2", "Data type failed.");
    Assert(frame.ServiceCode == 0x00, "Service Code failed.");
    Assert(frame.MessageCode == 0x22, "Message Code failed.");
    Assert((ushort)frame.Values["X"] == 32768, "X decode failed.");
    Assert((ushort)frame.Values["Y"] == 32767, "Y decode failed.");
}

static void BigEndianUShort2Encodes()
{
    var sample = new JoystickSample();
    sample.Axes["X"] = new AxisState { Name = "X", Raw = 0 };
    sample.Axes["Y"] = new AxisState { Name = "Y", Raw = -1 };
    var frame = new CANaerospaceEncoderService().EncodeJoystickSample(MapperProfile.CreateDefault(), sample)[0];
    Assert(frame.Payload[4] == 0x80 && frame.Payload[5] == 0x00, "X is not big-endian 0x8000.");
    Assert(frame.Payload[6] == 0x7F && frame.Payload[7] == 0xFF, "Y is not big-endian 0x7FFF.");
}

static void BLongButtonPackingEncodes()
{
    var sample = new JoystickSample();
    sample.Buttons[0] = true;
    sample.Buttons[31] = true;
    var frame = new CANaerospaceEncoderService().EncodeJoystickSample(MapperProfile.CreateDefault(), sample).Single(x => x.CanIdDecimal == 1504);
    Assert(frame.Payload[4] == 0x80 && frame.Payload[7] == 0x01, "Button BLONG packing failed.");
}

static void ServiceCodeFlagsDecode()
{
    var names = CANaerospaceServiceCode.Decode(0x03);
    Assert(names.Contains("data stale"), "Stale flag missing.");
    Assert(names.Contains("USB disconnected"), "USB flag missing.");
}

static void MessageCodeRollsOver()
{
    var encoder = new CANaerospaceEncoderService();
    byte code = 0;
    for (var i = 0; i < 257; i++)
        code = encoder.NextMessageCode(1500);
    Assert(code == 0, "Message Code did not wrap 255 -> 0.");
}

static void DecimalAndHexCanIdsDisplay()
{
    var frame = new CANaerospaceFrame { CanIdDecimal = 1500 };
    Assert(frame.CanIdHex == "0x5DC", "Hex display failed.");
}

static void MalformedCanLineDoesNotCrash()
{
    var decoded = new CANaerospaceDecoderService().DecodeLine("not a can frame");
    Assert(!decoded.IsValid, "Malformed line should be invalid.");
}

static void Assert(bool condition, string message)
{
    if (!condition)
        throw new InvalidOperationException(message);
}
