param(
    [string]$Target = "stm32_f407_usbhost_can_joystick",
    [switch]$All
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$targetDir = Join-Path $repoRoot "firmware\targets\$Target"

if (-not (Test-Path $targetDir)) {
    Write-Error "Target directory not found: $targetDir"
}

$patterns = @("*.elf")
if ($All) {
    $patterns += @("*.map", "*.list", "*.hex", "*.bin")
}

$deleted = 0
foreach ($pattern in $patterns) {
    $files = Get-ChildItem -Path $targetDir -Recurse -File -Filter $pattern -ErrorAction SilentlyContinue
    foreach ($file in $files) {
        Remove-Item -Path $file.FullName -Force
        $deleted++
        Write-Host "Deleted $($file.FullName)"
    }
}

Write-Host "Removed $deleted build artifact file(s)."
