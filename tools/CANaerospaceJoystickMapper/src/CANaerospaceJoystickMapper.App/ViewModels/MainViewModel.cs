using System.IO;
using CANaerospaceJoystickMapper.Core.Models;
using CANaerospaceJoystickMapper.Core.Services;
using Microsoft.Win32;

namespace CANaerospaceJoystickMapper.App.ViewModels;

public sealed class MainViewModel : ObservableObject
{
    private readonly ProfileService _profiles = new();
    private readonly DemoJoystickService _demo = new();
    private readonly SerialJoystickService _serial = new();
    private MapperProfile _profile = MapperProfile.CreateDefault();
    private bool _demoMode = true;
    private string _deviceStatus = "Demo Mode";

    public DashboardViewModel Dashboard { get; } = new();
    public MappingViewModel Mapping { get; } = new();
    public ProtocolViewModel Protocol { get; } = new();
    public ProfilesViewModel Profiles { get; } = new();
    public DiagnosticsViewModel Diagnostics { get; } = new();

    public MapperProfile Profile { get => _profile; set { if (Set(ref _profile, value)) OnProfileChanged(); } }
    public bool DemoMode
    {
        get => _demoMode;
        set
        {
            if (!Set(ref _demoMode, value)) return;
            Dashboard.Clear();
            DeviceStatus = value ? "Demo Mode" : "Disconnected";
        }
    }
    public string DeviceStatus { get => _deviceStatus; set => Set(ref _deviceStatus, value); }
    public string ProtocolVersion => "CANaerospace v1";
    public string NodeIdLabel => $"Node-ID {Profile.NodeId}";
    public string ProfileName { get => Profile.ProfileName; set { Profile.ProfileName = value; OnPropertyChanged(); } }

    public RelayCommand NewProfileCommand { get; }
    public RelayCommand SaveProfileCommand { get; }
    public RelayCommand LoadProfileCommand { get; }
    public RelayCommand ExportProfileCommand { get; }
    public RelayCommand ResetDefaultsCommand { get; }
    public RelayCommand ApplyCommand { get; }
    public RelayCommand DecodeCanCommand { get; }
    public RelayCommand RefreshPortsCommand { get; }
    public RelayCommand RefreshCANAdaptersCommand { get; }
    public RelayCommand ConnectCommand { get; }
    public RelayCommand ConnectCANCommand { get; }
    public RelayCommand AddManualLineCommand { get; }
    public RelayCommand AddManualCANLineCommand { get; }
    public RelayCommand CopyDiagnosticsCommand { get; }

    public MainViewModel()
    {
        Mapping.LoadFrom(Profile);
        Profiles.Validate(Profile);
        Diagnostics.DebugLineReceived += OnDebugLineReceived;
        Diagnostics.CANLineReceived += OnCANLineReceived;

        NewProfileCommand = new RelayCommand(NewProfile);
        SaveProfileCommand = new RelayCommand(SaveProfile);
        LoadProfileCommand = new RelayCommand(LoadProfile);
        ExportProfileCommand = new RelayCommand(SaveProfile);
        ResetDefaultsCommand = new RelayCommand(ResetDefaults);
        ApplyCommand = new RelayCommand(Apply);
        DecodeCanCommand = new RelayCommand(Protocol.Decode);
        RefreshPortsCommand = new RelayCommand(Diagnostics.RefreshPorts);
        RefreshCANAdaptersCommand = new RelayCommand(Diagnostics.RefreshCANAdapters);
        ConnectCommand = new RelayCommand(Diagnostics.ToggleConnection);
        ConnectCANCommand = new RelayCommand(Diagnostics.ToggleCANConnection);
        AddManualLineCommand = new RelayCommand(Diagnostics.AddManualLine);
        AddManualCANLineCommand = new RelayCommand(Diagnostics.AddManualCANLine);
        CopyDiagnosticsCommand = new RelayCommand(() => System.Windows.Clipboard.SetText(Diagnostics.CopyDiagnostics()));

        Protocol.Decode();
        Tick();
    }

    public void Tick()
    {
        if (DemoMode) Dashboard.Apply(_demo.Next());
        else Dashboard.RefreshAge();
    }

    private void OnProfileChanged()
    {
        OnPropertyChanged(nameof(ProfileName));
        OnPropertyChanged(nameof(NodeIdLabel));
        Mapping.LoadFrom(Profile);
        Apply();
    }

    private void Apply()
    {
        Mapping.WriteTo(Profile);
        if (!Profiles.Validate(Profile)) return;
        // The editor remains a draft until Apply. Keep the active preview isolated from later edits.
        var applied = _profiles.Deserialize(_profiles.Serialize(Profile));
        Dashboard.SetProfile(applied);
        Protocol.SetProfile(applied);
        OnPropertyChanged(nameof(NodeIdLabel));
    }

    private void NewProfile()
    {
        Profile = MapperProfile.CreateDefault();
    }

    private void ResetDefaults()
    {
        Profile = MapperProfile.CreateDefault();
    }

    private async void SaveProfile()
    {
        Mapping.WriteTo(Profile);
        if (!Profiles.Validate(Profile)) return;
        var dialog = new SaveFileDialog { Filter = "Mapper profile (*.json)|*.json", FileName = $"{Sanitize(Profile.ProfileName)}.json" };
        if (dialog.ShowDialog() != true) return;
        try { await _profiles.SaveAsync(dialog.FileName, Profile); }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            Profiles.ValidationSummary = $"Could not save profile. {ex.Message}";
        }
    }

    private void LoadProfile()
    {
        var dialog = new OpenFileDialog { Filter = "Mapper profile (*.json)|*.json" };
        if (dialog.ShowDialog() != true)
            return;
        try
        {
            var candidate = _profiles.Deserialize(File.ReadAllText(dialog.FileName));
            if (Profiles.Validate(candidate)) Profile = candidate;
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or System.Text.Json.JsonException)
        {
            Profiles.ValidationSummary = $"Could not load profile. {ex.Message}";
        }
    }

    private void OnDebugLineReceived(string line)
    {
        var sample = _serial.TryParseDebugLine(line);
        if (sample == null)
            return;
        DemoMode = false;
        DeviceStatus = "Manual replay";
        Dashboard.Apply(sample);
    }

    private void OnCANLineReceived(string line)
    {
        Protocol.DecoderInput = line;
        Protocol.Decode();
    }

    private static string Sanitize(string value)
    {
        foreach (var c in Path.GetInvalidFileNameChars())
            value = value.Replace(c, '_');
        return value;
    }
}
