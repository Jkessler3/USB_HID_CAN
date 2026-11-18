# App Layer

Non-generated translator glue for the active target.

`joystick_translator_app.[ch]` wires:

- USB host HID device state and report parsing (`shared/usb_host_hid`)
- joystick-to-CAN mapping and fixed-rate frame scheduling (`shared/mapping`)
- output `can_frame_t` generation for target CAN TX path

The generated target invokes the application from its USER CODE blocks.

## UART bench traces

Debug builds emit foreground USB events, HID JOY samples, CAN logs and a heartbeat on USART2 at 115200 8N1. Use Debug for the mapper's live UART preview. Release builds disable these synchronous traces to keep the USB/CAN scheduling loop free of UART transmission. USB status snapshots, CAN fault flags and foreground startup error reporting remain available in Release.

For a one-off optimized CLI bench build, run these commands at the repository root in PowerShell. The clean rebuild applies the changed compiler definition to every translation unit.

```powershell
$env:JOYSTICK_UART_TRACE = '1'
try {
    .\firmware\tools\build\build.ps1 -Config Release -Clean
} finally {
    Remove-Item Env:JOYSTICK_UART_TRACE
}
```

Run another clean Release build with the override unset to return to the production configuration. In CubeIDE, add `JOYSTICK_UART_TRACE=1` to the Release compiler preprocessor symbols for the bench build, then clean and rebuild. UART traces add scheduling latency while enabled. Measure acceptance timing using the quiet Release configuration.
