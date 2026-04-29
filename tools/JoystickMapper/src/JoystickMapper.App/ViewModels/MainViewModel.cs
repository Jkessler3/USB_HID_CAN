using System.IO;
using JoystickMapper.Core.Models;
using JoystickMapper.Core.Services;
using Microsoft.Win32;

namespace JoystickMapper.App.ViewModels;

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
    public bool DemoMode { get => _demoMode; set { if (Set(ref _demoMode, value)) DeviceStatus = value ? "Demo Mode" : "Disconnected"; } }
    public string DeviceStatus { get => _deviceStatus; set => Set(ref _deviceStatus, value); }
    public string ProtocolVersion => "Protocol v1";
    public string ProfileName { get => Profile.ProfileName; set { Profile.ProfileName = value; OnPropertyChanged(); } }

    public RelayCommand NewProfileCommand { get; }
    public RelayCommand SaveProfileCommand { get; }
    public RelayCommand LoadProfileCommand { get; }
    public RelayCommand ExportProfileCommand { get; }
    public RelayCommand ResetDefaultsCommand { get; }
    public RelayCommand ApplyCommand { get; }
    public RelayCommand DecodeCanCommand { get; }
    public RelayCommand RefreshPortsCommand { get; }
    public RelayCommand ConnectCommand { get; }
    public RelayCommand AddManualLineCommand { get; }
    public RelayCommand CopyDiagnosticsCommand { get; }

    public MainViewModel()
    {
        Mapping.LoadFrom(Profile);
        Profiles.Validate(Profile);
        Diagnostics.DebugLineReceived += OnDebugLineReceived;

        NewProfileCommand = new RelayCommand(NewProfile);
        SaveProfileCommand = new RelayCommand(SaveProfile);
        LoadProfileCommand = new RelayCommand(LoadProfile);
        ExportProfileCommand = new RelayCommand(SaveProfile);
        ResetDefaultsCommand = new RelayCommand(ResetDefaults);
        ApplyCommand = new RelayCommand(Apply);
        DecodeCanCommand = new RelayCommand(Protocol.Decode);
        RefreshPortsCommand = new RelayCommand(Diagnostics.RefreshPorts);
        ConnectCommand = new RelayCommand(Diagnostics.ToggleConnection);
        AddManualLineCommand = new RelayCommand(Diagnostics.AddManualLine);
        CopyDiagnosticsCommand = new RelayCommand(() => System.Windows.Clipboard.SetText(Diagnostics.CopyDiagnostics()));

        Protocol.Decode();
        Tick();
    }

    public void Tick()
    {
        if (DemoMode)
            Dashboard.Apply(_demo.Next());
    }

    private void OnProfileChanged()
    {
        OnPropertyChanged(nameof(ProfileName));
        Mapping.LoadFrom(Profile);
        Profiles.Validate(Profile);
    }

    private void Apply()
    {
        Mapping.WriteTo(Profile);
        Profiles.Validate(Profile);
        DeviceStatus = DemoMode ? "Demo Mode" : "Disconnected";
    }

    private void NewProfile()
    {
        Profile = MapperProfile.CreateDefault();
    }

    private void ResetDefaults()
    {
        Mapping.ResetDefaults();
        Mapping.WriteTo(Profile);
        Profiles.Validate(Profile);
    }

    private void SaveProfile()
    {
        Mapping.WriteTo(Profile);
        var dialog = new SaveFileDialog { Filter = "Mapper profile (*.json)|*.json", FileName = $"{Sanitize(Profile.ProfileName)}.json" };
        if (dialog.ShowDialog() == true)
            File.WriteAllText(dialog.FileName, _profiles.Serialize(Profile));
    }

    private void LoadProfile()
    {
        var dialog = new OpenFileDialog { Filter = "Mapper profile (*.json)|*.json" };
        if (dialog.ShowDialog() != true)
            return;
        Profile = _profiles.Deserialize(File.ReadAllText(dialog.FileName));
    }

    private void OnDebugLineReceived(string line)
    {
        var sample = _serial.TryParseDebugLine(line);
        if (sample == null)
            return;
        DemoMode = false;
        DeviceStatus = "Connected";
        Dashboard.Apply(sample);
    }

    private static string Sanitize(string value)
    {
        foreach (var c in Path.GetInvalidFileNameChars())
            value = value.Replace(c, '_');
        return value;
    }
}
