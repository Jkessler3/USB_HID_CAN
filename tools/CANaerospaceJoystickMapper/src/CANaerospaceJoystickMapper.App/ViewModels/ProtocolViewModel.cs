using System.Collections.ObjectModel;
using System.IO;
using CANaerospaceJoystickMapper.Core.Models;
using CANaerospaceJoystickMapper.Core.Services;

namespace CANaerospaceJoystickMapper.App.ViewModels;

public sealed class ProtocolViewModel : ObservableObject
{
    private CANaerospaceDecoderService _decoder = new();
    private string _decoderInput = "1500#010D002280007FFF\r\n1506#0110002200220000";
    private string _decodeWarnings = "";

    public string DecoderInput
    {
        get => _decoderInput;
        set
        {
            if (!Set(ref _decoderInput, value)) return;
            DecodedRows.Clear();
            DecodeWarnings = "";
        }
    }
    public string DecodeWarnings { get => _decodeWarnings; set => Set(ref _decodeWarnings, value); }
    public ObservableCollection<DecodedRow> DecodedRows { get; } = [];

    public void SetProfile(MapperProfile profile)
    {
        _decoder = new CANaerospaceDecoderService(profile);
        Decode();
    }

    public void Decode()
    {
        DecodedRows.Clear();
        _decoder.ResetSequence();
        int lineNumber = 0, invalid = 0, warnings = 0;
        using var capture = new StringReader(DecoderInput ?? "");
        while (capture.ReadLine() is { } line)
        {
            ++lineNumber;
            if (string.IsNullOrWhiteSpace(line)) continue;
            var decoded = _decoder.DecodeLine(line);
            string state = "OK";
            if (!decoded.IsValid)
            {
                ++invalid;
                state = "Invalid";
            }
            else if (decoded.Frame!.Warnings.Count > 0)
            {
                ++warnings;
                state = "Warning";
            }
            DecodedRows.Add(new DecodedRow(decoded.FrameType, state, $"Line {lineNumber}: {decoded.Summary}"));
        }

        DecodeWarnings = DecodedRows.Count == 0 ? "No CAN frames to decode."
            : $"{DecodedRows.Count - invalid} decoded, {invalid} invalid, {warnings} with warnings.";
    }
}

public sealed record DecodedRow(string Type, string State, string Summary);
