using System.Collections.ObjectModel;
using JoystickMapper.Core.Services;

namespace JoystickMapper.App.ViewModels;

public sealed class ProtocolViewModel : ObservableObject
{
    private readonly CanDecoderService _decoder = new();
    private string _decoderInput = "301#0000100000000142\r\n302#0304421001020304";
    private string _decodeWarnings = "";

    public string DecoderInput { get => _decoderInput; set => Set(ref _decoderInput, value); }
    public string DecodeWarnings { get => _decodeWarnings; set => Set(ref _decodeWarnings, value); }
    public ObservableCollection<DecodedRow> DecodedRows { get; } = [];

    public void Decode()
    {
        DecodedRows.Clear();
        byte? lastControlSample = null;
        byte? lastStatusSample = null;
        foreach (var line in DecoderInput.Split(["\r\n", "\n"], StringSplitOptions.RemoveEmptyEntries))
        {
            var decoded = _decoder.DecodeLine(line);
            DecodedRows.Add(new DecodedRow(decoded.FrameType, decoded.IsValid ? "OK" : "Invalid", decoded.Summary));
            if (decoded.Control != null)
                lastControlSample = decoded.Control.SampleCounter;
            if (decoded.Status != null)
                lastStatusSample = decoded.Status.SampleCounter;
        }

        DecodeWarnings = lastControlSample.HasValue && lastStatusSample.HasValue && lastControlSample.Value != lastStatusSample.Value
            ? $"Sample counter mismatch: control={lastControlSample} status={lastStatusSample}"
            : "No counter mismatch detected.";
    }
}

public sealed record DecodedRow(string Type, string State, string Summary);
