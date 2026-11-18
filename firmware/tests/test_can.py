"""Build and run native regression tests against the firmware's CAN mapping sources."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    tests = Path(__file__).resolve().parent
    shared = tests.parent / "shared"
    compiler = os.environ.get("CC") or shutil.which("gcc")
    if not compiler:
        compiler = "C:/msys64/mingw64/bin/gcc.exe"
    resolved = shutil.which(compiler) or compiler
    env = os.environ.copy()
    env["PATH"] = str(Path(resolved).resolve().parent) + os.pathsep + env["PATH"]
    modules = ("mapping", "can", "usb_host_hid")
    flags = [f"-I{shared / module / 'include'}" for module in modules]
    sources = [p for module in modules for p in sorted((shared / module / "src").glob("*.c"))]
    with tempfile.TemporaryDirectory(prefix="joystick-can-") as scratch:
        exe = Path(scratch) / ("test_can.exe" if os.name == "nt" else "test_can")
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                        *flags, tests / "test_can.c", *sources, "-o", exe], env=env, check=True)
        subprocess.run([exe], env=env, check=True)
        target = tests.parent / "targets/stm32_f407_usbhost_can_joystick"
        target_includes = (
            "App", "Core/Inc", "USB_HOST/App", "USB_HOST/Target",
            "Drivers/STM32F4xx_HAL_Driver/Inc", "Drivers/CMSIS/Include",
            "Drivers/CMSIS/Device/ST/STM32F4xx/Include",
            "Middlewares/ST/STM32_USB_Host_Library/Core/Inc",
            "Middlewares/ST/STM32_USB_Host_Library/Class/HID/Inc",
        )
        target_options = [compiler, "-std=gnu11", "-O2", "-ffunction-sections", "-fdata-sections",
                        "-Wl,--gc-sections", "-DSTM32F407xx", "-DUSE_HAL_DRIVER",
                        "-Wno-int-to-pointer-cast", "-Wno-pointer-to-int-cast",
                        *flags, *[f"-I{target / path}" for path in target_includes],
                        tests / "test_can_target.c", *sources, "-o", exe]
        for defines, tracing in (([], False), (["-DDEBUG"], True), (["-DJOYSTICK_UART_TRACE=1"], True)):
            subprocess.run([*target_options, *defines, f"-DEXPECT_UART_TRACE={int(tracing)}"], env=env, check=True)
            run = subprocess.run([exe], env=env, capture_output=True, text=True)
            assert run.returncode == 0, (defines, run.stderr, run.stdout)
            for marker in ("HID RAW ", "HID JOY ", "CAN TX ca ", "USB APP state "):
                assert (marker in run.stdout) == tracing, (defines, marker, run.stdout)
            print(f"Target CAN and UART trace checks passed ({defines or ['Release']})")


if __name__ == "__main__":
    main()
