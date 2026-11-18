using System.Text.Json;
using CANaerospaceJoystickMapper.Core.Models;

namespace CANaerospaceJoystickMapper.Core.Services;

public sealed class ProfileService
{
    private static readonly JsonSerializerOptions Options = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = true
    };

    public string Serialize(MapperProfile profile) => JsonSerializer.Serialize(profile, Options);

    public MapperProfile Deserialize(string json)
    {
        var profile = JsonSerializer.Deserialize<MapperProfile>(json, Options);
        return profile ?? throw new JsonException("Expected a mapper profile object.");
    }

    public async Task SaveAsync(string path, MapperProfile profile, CancellationToken cancellationToken = default)
    {
        var errors = profile.Validate();
        if (errors.Count != 0) throw new InvalidDataException(string.Join(Environment.NewLine, errors));
        string json = Serialize(profile);
        string target = Path.GetFullPath(path);
        string directory = Path.GetDirectoryName(target)!;
        Directory.CreateDirectory(directory);
        string temporary = Path.Combine(directory, $".{Guid.NewGuid():N}.tmp");
        try
        {
            await File.WriteAllTextAsync(temporary, json, cancellationToken).ConfigureAwait(false);
            cancellationToken.ThrowIfCancellationRequested();
            File.Move(temporary, target, overwrite: true);
        }
        finally { File.Delete(temporary); }
    }

    public async Task<MapperProfile> LoadAsync(string path, CancellationToken cancellationToken = default)
    {
        var json = await File.ReadAllTextAsync(path, cancellationToken);
        return Deserialize(json);
    }
}
