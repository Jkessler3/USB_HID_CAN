param(
    [switch]$BuildFirst,
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$publishDir = Join-Path $root "dist\publish"
$exePath = Join-Path $publishDir "CANaerospace Joystick Mapper.exe"

if ($BuildFirst -or -not (Test-Path $exePath)) {
    & (Join-Path $root "build.ps1") -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
}

if (-not (Test-Path $exePath)) {
    throw "Published executable was not found at $exePath"
}

$shell = New-Object -ComObject WScript.Shell
$desktop = [Environment]::GetFolderPath("Desktop")
$startMenu = Join-Path ([Environment]::GetFolderPath("Programs")) "CANaerospace Joystick Mapper"

New-Item -ItemType Directory -Force -Path $startMenu | Out-Null

foreach ($shortcutPath in @(
    (Join-Path $desktop "CANaerospace Joystick Mapper.lnk"),
    (Join-Path $startMenu "CANaerospace Joystick Mapper.lnk")
)) {
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = $exePath
    $shortcut.WorkingDirectory = $publishDir
    $shortcut.Description = "CANaerospace Joystick Mapper"
    $shortcut.Save()
}

Write-Host "Shortcuts installed:"
Write-Host "  Desktop: $(Join-Path $desktop 'CANaerospace Joystick Mapper.lnk')"
Write-Host "  Start Menu: $(Join-Path $startMenu 'CANaerospace Joystick Mapper.lnk')"
