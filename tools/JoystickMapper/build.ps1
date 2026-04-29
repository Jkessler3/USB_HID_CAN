param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$coreProject = Join-Path $root "src\JoystickMapper.Core\JoystickMapper.Core.csproj"
$appProject = Join-Path $root "src\JoystickMapper.App\JoystickMapper.App.csproj"
$testProject = Join-Path $root "tests\JoystickMapper.Tests\JoystickMapper.Tests.csproj"
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

$info = & dotnet --info 2>&1
if ($LASTEXITCODE -ne 0 -or ($info -match "No SDKs were found")) {
    Write-Error ".NET SDK not found. Install the .NET 8 SDK or newer. The runtime alone is not enough to build."
}

Invoke-Checked { dotnet restore $appProject }
Invoke-Checked { dotnet restore $testProject }
Invoke-Checked { dotnet build $coreProject -c $Configuration --no-restore }
Invoke-Checked { dotnet build $testProject -c $Configuration --no-restore }
Invoke-Checked { dotnet build $appProject -c $Configuration --no-restore }

if (-not $SkipTests) {
    Invoke-Checked { dotnet run --project $testProject -c $Configuration --no-build }
}

if (Test-Path $publishDir) {
    Remove-Item $publishDir -Recurse -Force
}

Invoke-Checked {
    dotnet publish $appProject `
        -c $Configuration `
        -r win-x64 `
        --self-contained true `
        -p:PublishSingleFile=false `
        -p:IncludeNativeLibrariesForSelfExtract=true `
        -o $publishDir
}

Write-Host "Published CAN-USB Joystick Mapper to $publishDir"
