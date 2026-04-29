using System.Collections.ObjectModel;
using JoystickMapper.Core.Models;

namespace JoystickMapper.App.ViewModels;

public sealed class DashboardViewModel : ObservableObject
{
    public ObservableCollection<AxisState> Axes { get; } = [];
    public ObservableCollection<ButtonIndicator> Buttons { get; } = [];
    public ObservableCollection<StatusFlagItem> StatusFlags { get; } = [];

    private int _hat = -1;
    private byte _sampleCounter;
    private byte _frameCounter;
    private uint _lastReportMs;
    private string _lastReportAge = "n/a";

    public int Hat { get => _hat; set => Set(ref _hat, value); }
    public byte SampleCounter { get => _sampleCounter; set => Set(ref _sampleCounter, value); }
    public byte FrameCounter { get => _frameCounter; set => Set(ref _frameCounter, value); }
    public uint LastReportMs { get => _lastReportMs; set => Set(ref _lastReportMs, value); }
    public string LastReportAge { get => _lastReportAge; set => Set(ref _lastReportAge, value); }

    public DashboardViewModel()
    {
        foreach (var name in new[] { "X", "Y", "Z", "Rx", "Ry", "Rz", "Slider", "Dial" })
            Axes.Add(new AxisState { Name = name });
        for (var i = 0; i < 32; i++)
            Buttons.Add(new ButtonIndicator(i));
        foreach (var name in new[] { "Joystick Present", "HID Sample Valid", "Data Stale", "TX Backpressure Seen", "HID Parse Error", "USB Disconnected" })
            StatusFlags.Add(new StatusFlagItem(name));
    }

    public void Apply(JoystickSample sample)
    {
        for (var axisIndex = 0; axisIndex < Axes.Count; axisIndex++)
        {
            var axis = Axes[axisIndex];
            if (!sample.Axes.TryGetValue(axis.Name, out var next))
                continue;
            Axes[axisIndex] = new AxisState { Name = axis.Name, Raw = next.Raw, Normalized = next.Normalized };
        }

        for (var i = 0; i < Buttons.Count && i < sample.Buttons.Length; i++)
            Buttons[i].IsPressed = sample.Buttons[i];

        Hat = sample.Hat;
        SampleCounter = sample.SampleCounter;
        FrameCounter = sample.CanFrameCounter;
        LastReportMs = sample.LastReportMs;
        LastReportAge = $"{Math.Max(0, (DateTimeOffset.Now - sample.UpdatedAt).TotalMilliseconds):0} ms";

        SetStatus("Joystick Present", sample.JoystickPresent);
        SetStatus("HID Sample Valid", sample.HidSampleValid);
        SetStatus("Data Stale", sample.DataStale);
        SetStatus("TX Backpressure Seen", sample.TxBackpressureSeen);
        SetStatus("HID Parse Error", sample.HidParseError);
        SetStatus("USB Disconnected", sample.UsbDisconnected);
    }

    private void SetStatus(string name, bool active)
    {
        var item = StatusFlags.FirstOrDefault(x => x.Name == name);
        if (item != null)
            item.IsActive = active;
    }
}

public sealed class ButtonIndicator(int index) : ObservableObject
{
    private bool _isPressed;
    public int Index { get; } = index;
    public bool IsPressed { get => _isPressed; set => Set(ref _isPressed, value); }
    public string Label => (Index + 1).ToString("00");
}

public sealed class StatusFlagItem(string name) : ObservableObject
{
    private bool _isActive;
    public string Name { get; } = name;
    public bool IsActive { get => _isActive; set => Set(ref _isActive, value); }
}
