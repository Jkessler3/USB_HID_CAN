using System.Text.Json;
using JoystickMapper.Core.Models;

namespace JoystickMapper.Core.Services;

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
        return profile ?? MapperProfile.CreateDefault();
    }

    public async Task SaveAsync(string path, MapperProfile profile, CancellationToken cancellationToken = default)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path) ?? ".");
        await File.WriteAllTextAsync(path, Serialize(profile), cancellationToken);
    }

    public async Task<MapperProfile> LoadAsync(string path, CancellationToken cancellationToken = default)
    {
        var json = await File.ReadAllTextAsync(path, cancellationToken);
        return Deserialize(json);
    }
}
