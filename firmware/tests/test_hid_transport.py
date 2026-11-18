"""Exercise the production HID class with simulated host-controller transfers."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

def main():
    tests = Path(__file__).resolve().parent
    target = tests.parent / "targets" / "stm32_f407_usbhost_can_joystick"
    core = target / "Middlewares/ST/STM32_USB_Host_Library/Core"
    cc = os.environ.get("CC") or shutil.which("gcc")
    if not cc:
        mingw = Path("C:/msys64/mingw64/bin/gcc.exe")
        if not mingw.is_file():
            raise SystemExit("Set CC to a native GCC compiler or put gcc on PATH.")
        cc = str(mingw)
    env = os.environ.copy()
    compiler_path = shutil.which(cc) or cc
    env["PATH"] = str(Path(compiler_path).resolve().parent) + os.pathsep + env["PATH"]
    includes = [
        core / "Inc", core.parent / "Class/HID/Inc", target / "USB_HOST/Target", target / "Core/Inc",
        target / "Drivers/STM32F4xx_HAL_Driver/Inc",
        target / "Drivers/CMSIS/Device/ST/STM32F4xx/Include",
        target / "Drivers/CMSIS/Include",
    ]
    with tempfile.TemporaryDirectory(prefix="usbh-hid-") as temp:
        cases = [
            ("test_hid_transport", core.parent / "Class/HID/Src/usbh_hid.c"),
            ("test_hid_config", core / "Src/usbh_ctlreq.c"),
        ]
        for name, production_source in cases:
            exe = Path(temp) / (name + (".exe" if os.name == "nt" else ""))
            subprocess.run([
                cc, "-std=c11", "-O2", "-DSTM32F407xx", "-DUSE_HAL_DRIVER",
                "-Wno-int-to-pointer-cast", "-Wno-pointer-to-int-cast",
                *(f"-I{path}" for path in includes),
                str(tests / (name + ".c")), str(production_source), "-o", str(exe),
            ], env=env, check=True)
            subprocess.run([str(exe)], env=env, check=True)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
