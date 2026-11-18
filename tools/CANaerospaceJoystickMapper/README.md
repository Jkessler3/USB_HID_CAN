# CANaerospace Joystick Mapper

User-facing Windows mapper/configuration utility for the USB-HID joystick-to-CANaerospace project.

The app launches in Demo Mode without hardware. It shows live-looking joystick axes/buttons, the CANaerospace Node-ID, service-code validity state, editable CANaerospace parameter mappings, JSON profile import/export, UART diagnostics, and a CANaerospace frame decoder.

## Protocol

The primary protocol is CANaerospace, not the old proprietary `0x301` / `0x302` debug layout.

Every frame is classic CAN DLC 8:

| Byte | Field |
| --- | --- |
| 0 | Node-ID |
| 1 | Data Type |
| 2 | Service Code |
| 3 | Message Code |
| 4..7 | Big-endian parameter data |

Default project-local joystick identifiers are decimal `1500..1506`, hex `0x5DC..0x5E2`. The UI displays both forms to avoid decimal/hex confusion.

## Build and Run

From this folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Release
.\run-mapper.cmd
```

The build script restores packages, builds the core/app/tests, runs tests, and publishes a self-contained win-x64 app under `dist\publish`.

For a normal Windows shortcut install without building a full setup package:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\install-local.ps1 -BuildFirst
```

That creates Desktop and Start Menu shortcuts pointing at the published executable.

If the app is already built, use the no-build shortcut installer instead:

```powershell
.\install-local.cmd
```

Close the mapper before running `build.ps1` or `build-installer.ps1`; Windows locks files in `dist\publish` while the app is open.

## Profiles

Profiles are JSON files containing:

- protocol: `CANaerospace`
- transmitting Node-ID
- identifier assignment metadata
- service-code policy
- CANaerospace parameters
- editable axis and button mappings

Apply validates the draft and updates the local frame preview and capture decoder. The preview produces a frame set for each demo or manually replayed sample. Bus transmit rates and on-device configuration are firmware settings. Saving writes the local JSON profile.

An axis or button row at its factory CAN ID follows the ID of its named frame. An explicitly changed row ID routes that signal separately. Disabled rows contribute neutral axes or clear button bits. Disabling a frame suppresses the named group. The mapper checks standard IDs, data types and overlapping lanes or bits before applying. The default button format remains 32 big-endian BLONG bits per frame.

Axis calibration applies center offset and deadzone before mapping the input range to the output range. Invert reverses that range. Scale and offset adjust the mapped output before the final clamp. New profiles use zero deadzone to preserve the default wire values. Existing JSON files retain their stored deadzone.

## Diagnostics

Diagnostics replays pasted input offline. Capture UART output with your serial terminal, paste it into **UART replay**, and select **Replay UART lines**. Recognized joystick samples update the dashboard. All nonblank lines remain in the log for inspection.

Capture CAN traffic with your analyzer, paste one or more frames into **CAN capture decoder**, and select **Decode pasted capture**. The Protocol tab shows the decoded frames and errors. Each decode starts a new capture and tracks counters separately for each node and identifier. For example, `can0 5DC [8] 01 0D 00 22 80 00 7F FF` contains X = 32768 and Y = 32767 in the default profile.

**Check input support** reports availability for the selected adapter or replay format. The current build acquires input through paste and demo generation. Live serial, PCAN, Kvaser and CANable/SLCAN capture require a separate terminal or analyzer.

## Installer

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\installer\build-installer.ps1 -Configuration Release
```

If Inno Setup is available as `ISCC.exe`, the installer is written to:

```text
dist\CANaerospace_Joystick_Mapper_Setup.exe
```

If the compiler is missing, the script fails clearly with:

```text
Inno Setup compiler not found. Install Inno Setup or add ISCC.exe to PATH.
```
