param(
    [string]$Target = "stm32_f407_usbhost_can_joystick",
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [switch]$Clean,
    [string]$CubeIdeRoot
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$buildDir = Join-Path $repoRoot "firmware\targets\$Target\$Config"

if (-not (Test-Path $buildDir)) {
    Write-Error "Build directory not found: $buildDir"
}

function Resolve-ToolCommand {
    param(
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($cmd -and $cmd.Source) {
            return $cmd.Source
        }
    }

    return $null
}

function Add-PathDir {
    param(
        [string]$Dir
    )

    if (-not $Dir -or -not (Test-Path $Dir)) {
        return
    }

    $pathEntries = $env:Path -split ';'
    if ($pathEntries -contains $Dir) {
        return
    }

    $env:Path = "$Dir;$env:Path"
}

function Add-UniquePath {
    param(
        [System.Collections.Generic.List[string]]$List,
        [string]$PathItem
    )

    if (-not $PathItem) {
        return
    }

    $resolved = (Resolve-Path $PathItem -ErrorAction SilentlyContinue)
    if (-not $resolved) {
        return
    }

    $full = $resolved.Path
    if (-not $List.Contains($full)) {
        $List.Add($full)
    }
}

function Get-CubeIdeRoots {
    param(
        [string]$PreferredRoot
    )

    $roots = New-Object System.Collections.Generic.List[string]

    Add-UniquePath -List $roots -PathItem $PreferredRoot
    Add-UniquePath -List $roots -PathItem $env:STM32CUBEIDE_DIR
    Add-UniquePath -List $roots -PathItem $env:STM32CUBEIDE_PATH

    $searchBases = @(
        "C:\ST",
        (Join-Path $env:ProgramFiles "STMicroelectronics"),
        (Join-Path ${env:ProgramFiles(x86)} "STMicroelectronics")
    )

    foreach ($base in $searchBases) {
        if (-not $base -or -not (Test-Path $base)) {
            continue
        }

        foreach ($dir in (Get-ChildItem -Path $base -Directory -Filter "STM32CubeIDE*" -ErrorAction SilentlyContinue)) {
            Add-UniquePath -List $roots -PathItem $dir.FullName
        }
    }

    return $roots
}

function Find-CubeIdeTool {
    param(
        [System.Collections.Generic.List[string]]$IdeRoots,
        [string]$PluginGlob,
        [string]$ExeName
    )

    foreach ($ideRoot in $IdeRoots) {
        $pluginRoots = @(
            (Join-Path $ideRoot "STM32CubeIDE\plugins"),
            (Join-Path $ideRoot "plugins")
        )

        foreach ($pluginRoot in $pluginRoots) {
            if (-not (Test-Path $pluginRoot)) {
                continue
            }

            $pluginDirs = Get-ChildItem -Path (Join-Path $pluginRoot $PluginGlob) -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending

            foreach ($pluginDir in $pluginDirs) {
                $binDir = Join-Path $pluginDir.FullName "tools\bin"
                $exePath = Join-Path $binDir $ExeName
                if (Test-Path $exePath) {
                    return @{
                        ExePath = $exePath
                        BinDir = $binDir
                        PluginDir = $pluginDir.FullName
                        IdeRoot = $ideRoot
                    }
                }
            }
        }
    }

    return $null
}

$makeCandidates = @("make", "mingw32-make", "gmake")
$makeCmd = Resolve-ToolCommand -Candidates $makeCandidates
$gccCmd = Resolve-ToolCommand -Candidates @("arm-none-eabi-gcc")

if (-not $makeCmd -or -not $gccCmd) {
    $cubeIdeRoots = Get-CubeIdeRoots -PreferredRoot $CubeIdeRoot
    $cubeIdeDetected = $false

    if ($cubeIdeRoots.Count -gt 0) {
        if (-not $makeCmd) {
            $makeInfo = Find-CubeIdeTool -IdeRoots $cubeIdeRoots -PluginGlob "com.st.stm32cube.ide.mcu.externaltools.make.win32_*" -ExeName "make.exe"
            if ($makeInfo) {
                Add-PathDir -Dir $makeInfo.BinDir
                $cubeIdeDetected = $true
            }
        }

        if (-not $gccCmd) {
            $gccInfo = Find-CubeIdeTool -IdeRoots $cubeIdeRoots -PluginGlob "com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*" -ExeName "arm-none-eabi-gcc.exe"
            if ($gccInfo) {
                Add-PathDir -Dir $gccInfo.BinDir
                $cubeIdeDetected = $true
            }
        }

        if ($cubeIdeDetected) {
            Write-Host "Auto-detected STM32CubeIDE toolchain and updated PATH for this session."
        }
    }

    if (-not $makeCmd) {
        $makeCmd = Resolve-ToolCommand -Candidates $makeCandidates
    }
    if (-not $gccCmd) {
        $gccCmd = Resolve-ToolCommand -Candidates @("arm-none-eabi-gcc")
    }
}

if (-not $makeCmd) {
    Write-Error "No make tool found. Install make/mingw32-make/gmake or pass -CubeIdeRoot to your STM32CubeIDE install."
}

if (-not $gccCmd) {
    Write-Error "arm-none-eabi-gcc not found. Install GNU Arm Embedded toolchain or pass -CubeIdeRoot to your STM32CubeIDE install."
}

Write-Host "Building target=$Target config=$Config"
Write-Host "Using make: $makeCmd"
Write-Host "Using gcc : $gccCmd"

Push-Location $buildDir
try {
    if ($Clean) {
        & $makeCmd clean
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    & $makeCmd all
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
finally {
    Pop-Location
}
