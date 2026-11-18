using System.Collections.ObjectModel;
using CANaerospaceJoystickMapper.Core.Models;

namespace CANaerospaceJoystickMapper.App.ViewModels;

public sealed class MappingViewModel : ObservableObject
{
    public ObservableCollection<AxisMapping> Axes { get; } = [];
    public ObservableCollection<ButtonMapping> Buttons { get; } = [];

    public void LoadFrom(MapperProfile profile)
    {
        Axes.Clear();
        Buttons.Clear();
        foreach (var axis in profile.Axes)
            Axes.Add(axis);
        foreach (var button in profile.Buttons)
            Buttons.Add(button);
    }

    public void WriteTo(MapperProfile profile)
    {
        profile.Axes = Axes.ToList();
        profile.Buttons = Buttons.ToList();
    }

    public void ResetDefaults()
    {
        Axes.Clear();
        Buttons.Clear();
        foreach (var axis in AxisMapping.CreateDefaults())
            Axes.Add(axis);
        foreach (var button in ButtonMapping.CreateDefaults())
            Buttons.Add(button);
    }
}
