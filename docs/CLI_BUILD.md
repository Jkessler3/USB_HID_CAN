# CLI Build (No STM32CubeIDE UI)

Use PowerShell scripts that call the Cube-generated `makefile` under the target `Debug/` or `Release/` folder.

## Build

From repo root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\firmware\tools\build\build.ps1 -Target stm32_f407_usbhost_can_joystick -Config Debug
```

Clean + build:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\firmware\tools\build\build.ps1 -Target stm32_f407_usbhost_can_joystick -Config Debug -Clean
```

## Toolchain detection

`build.ps1` first checks your current `PATH`.  
If tools are missing, it auto-detects STM32CubeIDE bundled tools from common install locations:

- `C:\ST\STM32CubeIDE_*`
- `%ProgramFiles%\STMicroelectronics\STM32CubeIDE*`
- `%ProgramFiles(x86)%\STMicroelectronics\STM32CubeIDE*`

If your install is elsewhere, pass it explicitly:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\firmware\tools\build\build.ps1 -Target stm32_f407_usbhost_can_joystick -Config Debug -CubeIdeRoot "D:\Tools\STM32CubeIDE_1.19.0"
```

If auto-detection still fails, install/provide:

- `arm-none-eabi-gcc`
- one of: `make`, `mingw32-make`, or `gmake`

## Clean artifacts

Remove only `.elf` files:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\firmware\tools\build\clean_artifacts.ps1 -Target stm32_f407_usbhost_can_joystick
```

Remove `.elf`, `.map`, `.list`, `.hex`, `.bin`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\firmware\tools\build\clean_artifacts.ps1 -Target stm32_f407_usbhost_can_joystick -All
```
