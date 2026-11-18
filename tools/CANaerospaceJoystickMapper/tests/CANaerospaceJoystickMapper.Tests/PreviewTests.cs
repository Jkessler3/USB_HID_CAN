using System.IO;
using CANaerospaceJoystickMapper.App.ViewModels;
using CANaerospaceJoystickMapper.Core.Models;
using CANaerospaceJoystickMapper.Core.Services;

internal static class PreviewTests
{
    private static void Check(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }

    public static void ReportCounts()
    {
        var parser = new SerialJoystickService();
        var report = parser.TryParseDebugLine("HID JOY vid=0450 pid=0001 proto=0 rid=2 off=1 ax=[0,1,2,3,4,5,-32768,32767] btn=8000000000000001 bc=64 hat=7")!;
        Check(report.Axes.Count == 8 && report.Axes["Dial"].Normalized == 1000 && report.Axes["Slider"].Normalized == -1000, "Full-width axis diagnostic lost");
        Check(report.Buttons.Length == 64 && report.Buttons[63] && report.Buttons[0] && report.Hat == 7, "Full-width buttons or hat lost");
        var sparse = parser.TryParseDebugLine("HID JOY ax=[] btn=0000000000000000 bc=0 hat=-1")!;
        Check(sparse.Axes.Count == 0 && sparse.Buttons.Length == 0 && sparse.Hat == -1, "Sparse report was fabricated");
        Check(parser.TryParseDebugLine("HID JOY ax=[10,bad] btn=0 bc=1 hat=0") == null, "Malformed report accepted");
    }

    public static void ProtectSavedProfile()
    {
        string path = Path.Combine(Path.GetTempPath(), $"mapper-profile-{Guid.NewGuid():N}.json");
        var service = new ProfileService();
        var profile = MapperProfile.CreateDefault();
        try
        {
            profile.NodeId = 7;
            service.SaveAsync(path, profile).GetAwaiter().GetResult();
            string previous = File.ReadAllText(path);
            profile.NodeId = 0;
            bool rejected = false;
            try { service.SaveAsync(path, profile).GetAwaiter().GetResult(); }
            catch (InvalidDataException) { rejected = true; }
            Check(rejected && File.ReadAllText(path) == previous && service.LoadAsync(path).GetAwaiter().GetResult().NodeId == 7,
                "Invalid save replaced a valid profile");
        }
        finally { File.Delete(path); }
    }

    public static void InvertedRangeAndScale()
    {
        var axis = new AxisMapping { InputMin = -1000, InputMax = 1000, OutputMin = 0, OutputMax = 1000, Invert = true };
        Check(axis.Apply(-1000) == 1000 && axis.Apply(0) == 500 && axis.Apply(1000) == 0, "Inversion lost the output range");
        axis.Scale = 0.5;
        axis.Offset = 20;
        Check(axis.Apply(0) == 270, "Scale and offset were ignored");
    }

    public static void ProfileRouting()
    {
        var profile = MapperProfile.CreateDefault();
        profile.NodeId = 9;
        profile.Parameters[0].CanIdDecimal = 1600;
        profile.Parameters[3].Enabled = false;
        profile.Axes[1].Enabled = false;
        profile.Buttons[31].Enabled = false;
        profile.Buttons[0].BitIndex = 31;
        profile.Buttons[0].ActiveHigh = false;
        var sample = new JoystickSample();
        sample.Axes["X"] = new AxisState { Raw = 2000 };
        sample.Axes["Y"] = new AxisState { Raw = 3000 };
        var frames = new CANaerospaceEncoderService().EncodeJoystickSample(profile, sample);
        var axes = frames.Single(x => x.CanIdDecimal == 1600);
        Check(axes.NodeId == 9 && axes.Data.SequenceEqual(new byte[] { 0x87, 0xD0, 0x80, 0x00 }), "Incorrect mapped axis pair");
        Check(!frames.Any(f => f.CanIdDecimal == 1503), "Disabled parameter still emitted");
        Check(frames.Single(x => x.CanIdDecimal == 1504).Data.SequenceEqual(new byte[] { 0x80, 0, 0, 0 }), "Active-low button was not rerouted");
        sample.StatusFlags = CANaerospaceServiceCode.DataStale;
        frames = new CANaerospaceEncoderService().EncodeJoystickSample(profile, sample);
        Check(frames.Single(f => f.CanIdDecimal == 1600).Data.SequenceEqual(new byte[] { 0x80,0,0x80,0 }), "Faulted axes were not neutral");
        Check(frames.Single(f => f.CanIdDecimal == 1504).Data.All(x => x == 0), "Faulted buttons were not cleared");
    }

    public static void InvalidEdits()
    {
        var p = MapperProfile.CreateDefault();
        p.Axes[0].CanIdDecimal = 1504;
        Check(p.Validate().Count > 0, "Mixed-type CAN ID accepted");
        p = MapperProfile.CreateDefault();
        p.Axes[0].DataBytePosition = 6;
        Check(p.Validate().Count > 0, "Duplicate lane accepted");
        p = MapperProfile.CreateDefault();
        p.Buttons[0].DataType = "USHORT2";
        Check(p.Validate().Count > 0, "Incompatible button type accepted");
    }

    public static void AppliedViewAndManualReplay()
    {
        var vm = new MainViewModel();
        vm.Profile.NodeId = 21;
        vm.Mapping.Axes[0].CanIdDecimal = 1599;
        vm.ApplyCommand.Execute(null);
        vm.Tick();
        Check(vm.Dashboard.Frames.Any(f => f.CanIdDecimal == 1599 && f.NodeId == 21), "Dashboard ignored applied edits");
        vm.Mapping.Axes[0].DataBytePosition = 7;
        vm.ApplyCommand.Execute(null);
        vm.Tick();
        Check(vm.Dashboard.Frames.Any(f => f.CanIdDecimal == 1599 && f.NodeId == 21), "Invalid edit changed the active preview");
        vm.Diagnostics.ManualLine = "HID JOY vid=044F pid=B10A proto=0 rid=1 off=1 ax=[0,120,-64,8] btn=00000001 bc=32 hat=2";
        vm.Diagnostics.AddManualLine();
        Check(vm.DeviceStatus.Contains("replay", StringComparison.OrdinalIgnoreCase) && !vm.DemoMode, "Manual report labelled as a live device");
        Check(vm.Dashboard.Axes.Single(a => a.Name == "Ry").Raw == 0 && vm.Dashboard.Buttons.Skip(32).All(b => !b.IsPressed), "Demo signals leaked into replay");
        Check(vm.Dashboard.Hat == 2, "Reported hat discarded");
        var loaded = new ProfileService().Deserialize(new ProfileService().Serialize(vm.Profile));
        Check(loaded.NodeId == 21 && loaded.Axes[0].CanIdDecimal == 1599, "Edited profile round trip failed");
    }
}
