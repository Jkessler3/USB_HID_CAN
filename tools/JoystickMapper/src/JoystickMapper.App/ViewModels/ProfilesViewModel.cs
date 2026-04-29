using JoystickMapper.Core.Models;

namespace JoystickMapper.App.ViewModels;

public sealed class ProfilesViewModel : ObservableObject
{
    private string _validationSummary = "Default profile is valid.";

    public string ValidationSummary { get => _validationSummary; set => Set(ref _validationSummary, value); }

    public void Validate(MapperProfile profile)
    {
        var errors = profile.Validate();
        ValidationSummary = errors.Count == 0 ? "Profile is valid." : string.Join(Environment.NewLine, errors);
    }
}
