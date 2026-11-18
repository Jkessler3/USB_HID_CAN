using CANaerospaceJoystickMapper.App.ViewModels;
using CANaerospaceJoystickMapper.Core.Models;
using CANaerospaceJoystickMapper.Core.Services;

internal static class DiagnosticsTests
{
    public static void Run()
    {
        PastedInputsStayOffline();
        CaptureSyntaxAndContract();
        SequenceHistoryBelongsToOneCapture();
        AppliedProfileDefinesExpectedType();
    }

    private static void PastedInputsStayOffline()
    {
        var adapterService = new CANAdapterService();
        var diagnostics = new DiagnosticsViewModel();
        foreach (var adapter in adapterService.ListAdapters())
        {
            Require(!adapterService.Connect(adapter, "sample", 500000).Connected,
                $"{adapter} reported a live connection without a driver");
            diagnostics.SelectedCANAdapter = adapter;
            diagnostics.ToggleCANConnection();
            Require(!diagnostics.CANConnected, $"{adapter} changed the live CAN state");
        }
        diagnostics.ToggleConnection();
        Require(!diagnostics.Connected, "UART reported a connection without opening a port");

        var received = new List<string>();
        diagnostics.DebugLineReceived += received.Add;
        diagnostics.ManualLine = "first UART line\r\n\nsecond UART line";
        diagnostics.AddManualLine();
        Require(received.SequenceEqual(new[] { "first UART line", "second UART line" }),
            "Pasted UART lines were lost or combined");

        const string capture = "5DC#010D000180008000\n5DC#010D000280008000";
        var pastedCaptures = new List<string>();
        diagnostics.CANLineReceived += pastedCaptures.Add;
        diagnostics.ManualCANLine = capture;
        diagnostics.AddManualCANLine();
        Require(pastedCaptures.SequenceEqual(new[] { capture }), "A single capture was split across decoder sessions");
        Require(!diagnostics.Connected && !diagnostics.CANConnected, "Pasting created a live connection");
    }

    private static void CaptureSyntaxAndContract()
    {
        var decoder = new CANaerospaceDecoderService();
        foreach (var line in new[]
        {
            "(123.456789) can0 5DC#010D000180007FFF",
            "(123.456789) can0 1500#010D000180007FFF",
            "can0  5DC   [8]  01 0D 00 02 80 00 7F FF",
            "1500#010D000380007FFF",
            "0x5DC#010D000480007FFF"
        })
        {
            var result = decoder.DecodeLine(line);
            Require(result.IsValid && result.Frame?.CanIdDecimal == 1500, $"Valid capture rejected: {line}");
        }
        foreach (var line in new[]
        {
            "can0  5DC   [7]  01 0D 00 05 80 00 7F FF",
            "can0  5DC   [8]  01 0D 00 05 80 00 7F",
            "can0  5DC   [8]  01 0D 00 05 80 00 7F FF A1",
            "can0  000005DC   [8]  01 0D 00 05 80 00 7F FF",
            "can0 0800#010D000580007FFF",
            "5DC#010D000580007FFFGG",
            "5DC#0105000580007FFF"
        })
            Require(!decoder.DecodeLine(line).IsValid, $"Malformed or incompatible capture accepted: {line}");
    }

    private static void SequenceHistoryBelongsToOneCapture()
    {
        var vm = new ProtocolViewModel { DecoderInput = "5DC#010D00FF80008000\n5DC#010D000080008000" };
        for (var i = 0; i < 2; i++)
        {
            vm.Decode();
            Require(vm.DecodedRows.Count == 2 && vm.DecodedRows.All(row => !ContainsGap(row.Summary)),
                "Replaying an intact capture or rolling over 255 introduced a false gap");
        }
        vm.DecoderInput = "5DC#010D000380008000\n5DC#020D006080008000\n5DC#0105000480008000\n5DC#010D000480008000\n5DC#010D000680008000";
        vm.Decode();
        Require(vm.DecodedRows.Count == 5 && vm.DecodedRows[2].State == "Invalid",
            "An invalid typed frame disappeared from the replay");
        Require(!ContainsGap(vm.DecodedRows[3].Summary) && ContainsGap(vm.DecodedRows[4].Summary),
            "Another node or an invalid packet changed the expected sequence");
        vm.DecoderInput = "";
        vm.Decode();
        Require(vm.DecodedRows.Count == 0, "Empty input retained rows from a previous capture");
    }

    private static void AppliedProfileDefinesExpectedType()
    {
        var profile = MapperProfile.CreateDefault();
        profile.Parameters[0].CanIdDecimal = 1600;
        profile.NodeId = 9;
        var vm = new ProtocolViewModel { DecoderInput = "1600#0905000180008000\n1600#090D000180008000" };
        vm.SetProfile(profile);
        vm.Decode();
        Require(vm.DecodedRows.Count == 2 && vm.DecodedRows[0].State == "Invalid"
                && vm.DecodedRows[1].State != "Invalid", "Decoder ignored the remapped X/Y contract");
    }

    private static bool ContainsGap(string value) => value.Contains("gap", StringComparison.OrdinalIgnoreCase);

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
