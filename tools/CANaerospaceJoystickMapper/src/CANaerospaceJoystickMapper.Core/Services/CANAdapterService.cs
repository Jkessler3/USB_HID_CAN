namespace CANaerospaceJoystickMapper.Core.Services;

public sealed class CANAdapterService
{
    public IReadOnlyList<string> ListAdapters() =>
    [
        "Decoder Only",
        "CANable / SLCAN",
        "PCAN",
        "Kvaser",
        "SocketCAN log"
    ];

    public CANConnectionResult Connect(string? adapter, string? channel, int bitrate)
    {
        adapter = string.IsNullOrWhiteSpace(adapter) ? "Decoder Only" : adapter.Trim();
        if (adapter.Equals("Decoder Only", StringComparison.OrdinalIgnoreCase) ||
            adapter.Equals("SocketCAN log", StringComparison.OrdinalIgnoreCase))
        {
            return new CANConnectionResult(false, "Offline decoder ready for pasted CAN frames.");
        }

        return new CANConnectionResult(
            false, $"Live capture for {adapter} is unavailable. Paste a capture from your analyzer.");
    }
}

public sealed record CANConnectionResult(bool Connected, string Message);
