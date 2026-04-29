param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")

& (Join-Path $root "build.ps1") -Configuration $Configuration

$iscc = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
if (-not $iscc) {
    $common = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
    )
    foreach ($candidate in $common) {
        if ($candidate -and (Test-Path $candidate)) {
            $iscc = Get-Item $candidate
            break
        }
    }
}

if (-not $iscc) {
    Write-Error "Inno Setup compiler not found. Install Inno Setup or add ISCC.exe to PATH."
}

$isccPath = if ($iscc.Source) { $iscc.Source } elseif ($iscc.Path) { $iscc.Path } else { $iscc.FullName }
& $isccPath (Join-Path $PSScriptRoot "JoystickMapper.iss")
Write-Host "Installer written to $(Join-Path $root 'dist\CAN_USB_Joystick_Mapper_Setup.exe')"
