"""Build and run the actual ST host core with a simulated USB peripheral."""
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
        core / "Inc", target / "USB_HOST/Target", target / "USB_HOST/App", target / "Core/Inc",
        target / "Middlewares/ST/STM32_USB_Host_Library/Class/HID/Inc",
        target / "Drivers/STM32F4xx_HAL_Driver/Inc",
        target / "Drivers/CMSIS/Device/ST/STM32F4xx/Include",
        target / "Drivers/CMSIS/Include",
    ]
    with tempfile.TemporaryDirectory(prefix="usbh-attach-") as temp:
        flags = [
            cc, "-std=c11", "-O2", "-DSTM32F407xx", "-DUSE_HAL_DRIVER",
            "-Wno-int-to-pointer-cast", "-Wno-pointer-to-int-cast",
            *(f"-I{path}" for path in includes),
        ]
        cases = {
            "test_attach": [str(tests / "test_attach.c"), *map(str, sorted((core / "Src").glob("*.c")))],
            # Discard unused HAL peripheral functions from the included platform translation unit.
            "test_attach_register": ["-fwhole-program", str(tests / "test_attach_register.c")],
        }
        for name, sources in cases.items():
            exe = Path(temp) / (name + (".exe" if os.name == "nt" else ""))
            subprocess.run([*flags, *sources, "-o", str(exe)], env=env, check=True)
            result = subprocess.run([str(exe)], env=env)
            if result.returncode:
                return result.returncode
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
