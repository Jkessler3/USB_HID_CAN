# CAN-USB Joystick Mapper

Windows Mapper / Configuration Utility for the USB-host joystick-to-CAN firmware.

This is the Step 4 user-facing mapper/config milestone. It is intentionally repo-local and does not modify firmware transport, USB HID parsing, or CAN publishing behavior.

## What It Does

- Shows a live-looking joystick dashboard in Demo Mode.
- Displays axes, buttons, hat state, status flags, counters, and last report timing.
- Edits axis and button mapping profiles.
- Saves, loads, imports, and exports JSON profiles.
- Decodes receiver-facing CAN frames:
  - `0x301` control
  - `0x302` status
- Provides a UART diagnostics pane for tolerant parsing of firmware debug lines such as `HID JOY ... ax=[...] btn=...`.
- Includes Windows publish and Inno Setup installer scripts.

## What Is Not Implemented Yet

- Persistent device-side config write.
- Direct CAN adapter control.
- Firmware-side profile upload.
- Guaranteed live serial streaming. The Diagnostics tab has port discovery and tolerant line parsing, but live serial transport should be finalized once the device config protocol exists.

Profiles are local JSON files for now. Device write should stay disabled until firmware explicitly supports safe persistent configuration.

## Project Layout

- `src/JoystickMapper.Core` - profile model, CAN decoder, demo data, serial debug parser
- `src/JoystickMapper.App` - WPF desktop app
- `tests/JoystickMapper.Tests` - small no-dependency test runner
- `installer` - Inno Setup script and installer build script

## Requirements

- Windows
- .NET 8 SDK or newer
- Optional: Inno Setup 6 for setup `.exe` generation

This repository environment currently has .NET runtimes but no .NET SDK, so the app could not be compiled by the agent here. On a normal Windows dev machine with the SDK installed, use the commands below.

## Run In Demo Mode

```powershell
cd tools\JoystickMapper
dotnet run --project .\src\JoystickMapper.App\JoystickMapper.App.csproj
```

The app starts in Demo Mode and animates joystick axes/buttons without hardware.

## Build And Test

```powershell
cd tools\JoystickMapper
.\build.ps1 -Configuration Release
```

The script:

1. Restores packages.
2. Builds the solution.
3. Runs the no-dependency tests.
4. Publishes a self-contained `win-x64` app to `dist\publish`.

To skip tests while iterating:

```powershell
.\build.ps1 -Configuration Release -SkipTests
```

## Build Installer

Install Inno Setup 6, then run:

```powershell
cd tools\JoystickMapper
.\installer\build-installer.ps1 -Configuration Release
```

Output:

```text
tools\JoystickMapper\dist\CAN_USB_Joystick_Mapper_Setup.exe
```

If Inno Setup is missing, the script exits with:

```text
Inno Setup compiler not found. Install Inno Setup or add ISCC.exe to PATH.
```

## Profiles

Profiles use JSON. Example:

```json
{
  "version": 1,
  "profileName": "Default Joystick Mapping",
  "device": {
    "name": "Generic HID Joystick",
    "vid": null,
    "pid": null
  },
  "can": {
    "controlFrameId": "0x301",
    "statusFrameId": "0x302",
    "receiverTimeoutMs": 250
  },
  "axes": [
    {
      "enabled": true,
      "source": "X",
      "destinationCanId": "0x301",
      "destinationSignal": "Steer",
      "byteOffset": 0,
      "bitLength": 16,
      "signed": true,
      "littleEndian": true,
      "invert": false,
      "deadzone": 512,
      "centerOffset": 0,
      "inputMin": -32768,
      "inputMax": 32767,
      "outputMin": -1000,
      "outputMax": 1000
    }
  ],
  "buttons": [
    {
      "enabled": true,
      "buttonIndex": 0,
      "destinationCanId": "0x301",
      "byteOffset": 6,
      "bitIndex": 0,
      "activeHigh": true
    }
  ]
}
```

## CAN Decoder

The Protocol / CAN tab accepts common analyzer formats:

```text
301#0000100000000142
302#0304421001020304
can0  301   [8]  00 00 10 00 00 00 01 42
```

Decoder behavior follows `docs/CAN_PROTOCOL.md`.

## UART Diagnostics

The Diagnostics tab can parse firmware debug lines like:

```text
HID JOY vid=044F pid=B10A proto=0 rid=1 off=1 ax=[0,120,-64,8] btn=00000003 bc=32 hat=0
```

Unknown lines remain visible in the raw log and do not crash parsing.
