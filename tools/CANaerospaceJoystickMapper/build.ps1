param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$coreProject = Join-Path $root "src\CANaerospaceJoystickMapper.Core\CANaerospaceJoystickMapper.Core.csproj"
$appProject = Join-Path $root "src\CANaerospaceJoystickMapper.App\CANaerospaceJoystickMapper.App.csproj"
$testProject = Join-Path $root "tests\CANaerospaceJoystickMapper.Tests\CANaerospaceJoystickMapper.Tests.csproj"
$publishDir = Join-Path $root "dist\publish"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [scriptblock]$Command
    )

    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE"
    }
}

if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) {
    Write-Error ".NET SDK not found. Install the .NET 8 SDK or newer."
}

$versionOutput = @(& dotnet --version 2>&1)
$sdkVersion = $null
if ($LASTEXITCODE -ne 0 -or $versionOutput.Count -eq 0 -or
    -not [Version]::TryParse(($versionOutput[-1].ToString().Split('-')[0]), [ref]$sdkVersion) -or $sdkVersion.Major -lt 8) {
    Write-Error "This build requires the .NET 8 SDK or newer."
}

Invoke-Checked { dotnet restore $appProject --disable-parallel -m:1 }
Invoke-Checked { dotnet restore $testProject --disable-parallel -m:1 }
Invoke-Checked { dotnet build $coreProject -c $Configuration --no-restore -m:1 --disable-build-servers }
Invoke-Checked { dotnet build $testProject -c $Configuration --no-restore -m:1 --disable-build-servers }
Invoke-Checked { dotnet build $appProject -c $Configuration --no-restore -m:1 --disable-build-servers }

if (-not $SkipTests) {
    Invoke-Checked { dotnet run --project $testProject -c $Configuration --no-build }
}

if (Test-Path $publishDir) {
    try {
        Remove-Item $publishDir -Recurse -Force
    } catch {
        Write-Error "Could not clean $publishDir. Close CANaerospace Joystick Mapper if it is running, then run the build again. Original error: $($_.Exception.Message)"
    }
}

Invoke-Checked {
    dotnet publish $appProject `
        -c $Configuration `
        -m:1 --disable-build-servers `
        -r win-x64 `
        --self-contained true `
        -p:PublishSingleFile=false `
        -p:IncludeNativeLibrariesForSelfExtract=true `
        -o $publishDir
}

Write-Host "Published CANaerospace Joystick Mapper to $publishDir"
