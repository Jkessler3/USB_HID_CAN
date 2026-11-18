using CANaerospaceJoystickMapper.Core.Models;

namespace CANaerospaceJoystickMapper.Core.Services;

public sealed class DemoJoystickService
{
    private readonly string[] _axisNames = ["X", "Y", "Z", "Rx", "Ry", "Rz", "Slider", "Dial"];
    private double _phase;
    private byte _sampleCounter;
    private byte _frameCounter;

    public JoystickSample Next()
    {
        _phase += 0.11;
        _sampleCounter++;
        _frameCounter += 2;

        var sample = new JoystickSample
        {
            Buttons = new bool[64],
            Hat = (int)((_phase * 2) % 8),
            StatusFlags = CANaerospaceServiceCode.Valid,
            SampleCounter = _sampleCounter,
            CanFrameCounter = _frameCounter,
            LastReportMs = (uint)Environment.TickCount,
            UpdatedAt = DateTimeOffset.Now
        };

        for (var i = 0; i < _axisNames.Length; i++)
        {
            var normalized = (int)Math.Round(Math.Sin(_phase + i * 0.7) * 1000);
            sample.Axes[_axisNames[i]] = new AxisState
            {
                Name = _axisNames[i],
                Raw = normalized * 32,
                Normalized = normalized
            };
        }

        for (var i = 0; i < sample.Buttons.Length; i++)
            sample.Buttons[i] = ((int)(_phase * 3) + i) % 11 == 0;

        return sample;
    }
}
