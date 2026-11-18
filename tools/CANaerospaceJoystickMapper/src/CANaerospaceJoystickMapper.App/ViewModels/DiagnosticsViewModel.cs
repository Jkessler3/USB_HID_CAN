using System.Collections.ObjectModel;
using CANaerospaceJoystickMapper.Core.Services;

namespace CANaerospaceJoystickMapper.App.ViewModels;

public sealed class DiagnosticsViewModel : ObservableObject
{
    private readonly SerialJoystickService _serial = new();
    private readonly CANAdapterService _can = new();
    private string? _selectedPort;
    private string? _selectedCANAdapter;
    private string _canChannel = "manual";
    private int _baudRate = 115200;
    private int _canBitrate = 500000;
    private string _manualLine = "HID JOY vid=044F pid=B10A proto=0 rid=1 off=1 ax=[0,120,-64,8] btn=00000003 bc=32 hat=0";
    private string _manualCANLine = "1500#010D002280007FFF";

    public ObservableCollection<string> Ports { get; } = [];
    public ObservableCollection<string> CANAdapters { get; } = [];
    public ObservableCollection<int> BaudRates { get; } = [9600, 57600, 115200, 230400];
    public ObservableCollection<int> CANBitrates { get; } = [125000, 250000, 500000, 1000000];
    public ObservableCollection<string> LogLines { get; } = [];
    public ObservableCollection<string> CANLogLines { get; } = [];
    public string? SelectedPort { get => _selectedPort; set => Set(ref _selectedPort, value); }
    public string? SelectedCANAdapter { get => _selectedCANAdapter; set => Set(ref _selectedCANAdapter, value); }
    public string CANChannel { get => _canChannel; set => Set(ref _canChannel, value); }
    public int BaudRate { get => _baudRate; set => Set(ref _baudRate, value); }
    public int CANBitrate { get => _canBitrate; set => Set(ref _canBitrate, value); }
    public bool Connected => false;
    public bool CANConnected => false;
    public string ManualLine { get => _manualLine; set => Set(ref _manualLine, value); }
    public string ManualCANLine { get => _manualCANLine; set => Set(ref _manualCANLine, value); }

    public event Action<string>? DebugLineReceived;
    public event Action<string>? CANLineReceived;

    public DiagnosticsViewModel()
    {
        RefreshCANAdapters();
    }

    public void RefreshPorts()
    {
        Ports.Clear();
        foreach (var port in _serial.ListPorts())
            Ports.Add(port);
        if (SelectedPort == null || !Ports.Contains(SelectedPort))
            SelectedPort = Ports.FirstOrDefault();
    }

    public void RefreshCANAdapters()
    {
        CANAdapters.Clear();
        foreach (var adapter in _can.ListAdapters())
            CANAdapters.Add(adapter);
        SelectedCANAdapter ??= CANAdapters.FirstOrDefault();
    }

    public void ToggleConnection()
    {
        LogLines.Add("Live UART capture is unavailable. Paste output from your serial terminal.");
    }

    public void ToggleCANConnection()
    {
        var result = _can.Connect(SelectedCANAdapter, CANChannel, CANBitrate);
        CANLogLines.Add(result.Message);
    }

    public void AddManualLine()
    {
        foreach (var line in NonblankLines(ManualLine))
        {
            LogLines.Add(line);
            DebugLineReceived?.Invoke(line);
        }
    }

    public void AddManualCANLine()
    {
        if (string.IsNullOrWhiteSpace(ManualCANLine))
            return;
        foreach (var line in NonblankLines(ManualCANLine))
            CANLogLines.Add(line);
        // Preserve a capture as one decoder session for per-identifier counters.
        CANLineReceived?.Invoke(ManualCANLine);
    }

    public string CopyDiagnostics() =>
        string.Join(Environment.NewLine,
            LogLines.Select(line => $"UART replay | {line}")
                .Concat(CANLogLines.Select(line => $"CAN replay | {line}")));

    private static IEnumerable<string> NonblankLines(string input) =>
        input.Split(["\r\n", "\r", "\n"], StringSplitOptions.RemoveEmptyEntries)
            .Where(line => !string.IsNullOrWhiteSpace(line));
}
