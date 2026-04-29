using System.Collections.ObjectModel;
using JoystickMapper.Core.Services;

namespace JoystickMapper.App.ViewModels;

public sealed class DiagnosticsViewModel : ObservableObject
{
    private readonly SerialJoystickService _serial = new();
    private string? _selectedPort;
    private int _baudRate = 115200;
    private bool _connected;
    private string _manualLine = "HID JOY vid=044F pid=B10A proto=0 rid=1 off=1 ax=[0,120,-64,8] btn=00000003 bc=32 hat=0";

    public ObservableCollection<string> Ports { get; } = [];
    public ObservableCollection<int> BaudRates { get; } = [9600, 57600, 115200, 230400];
    public ObservableCollection<string> LogLines { get; } = [];
    public string? SelectedPort { get => _selectedPort; set => Set(ref _selectedPort, value); }
    public int BaudRate { get => _baudRate; set => Set(ref _baudRate, value); }
    public bool Connected { get => _connected; set => Set(ref _connected, value); }
    public string ManualLine { get => _manualLine; set => Set(ref _manualLine, value); }
    public string ConnectionLabel => Connected ? "Connected" : "Disconnected";

    public event Action<string>? DebugLineReceived;

    public DiagnosticsViewModel()
    {
        RefreshPorts();
    }

    public void RefreshPorts()
    {
        Ports.Clear();
        foreach (var port in _serial.ListPorts())
            Ports.Add(port);
        SelectedPort ??= Ports.FirstOrDefault();
    }

    public void ToggleConnection()
    {
        Connected = !Connected;
        OnPropertyChanged(nameof(ConnectionLabel));
        LogLines.Add(Connected
            ? $"Connected to {SelectedPort ?? "demo serial"} at {BaudRate}. Live serial reading is stubbed until device config transport is finalized."
            : "Disconnected.");
    }

    public void AddManualLine()
    {
        if (string.IsNullOrWhiteSpace(ManualLine))
            return;
        LogLines.Add(ManualLine);
        DebugLineReceived?.Invoke(ManualLine);
    }

    public string CopyDiagnostics() => string.Join(Environment.NewLine, LogLines);
}
