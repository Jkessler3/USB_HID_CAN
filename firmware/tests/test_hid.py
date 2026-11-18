"""Compile and run portable HID profile/decoder regressions with a local C compiler."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

def test_hid():
    firmware = Path(__file__).resolve().parents[1]
    shared = firmware / "shared" / "usb_host_hid"
    compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
    if not compiler and os.name == "nt":
        local = Path("C:/msys64/mingw64/bin/gcc.exe")
        if local.exists():
            compiler = str(local)
    if not compiler:
        raise RuntimeError("Set CC to a C11 compiler")
    with tempfile.TemporaryDirectory() as temp:
        output = Path(temp) / ("hid_tests.exe" if os.name == "nt" else "hid_tests")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env["PATH"]
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(shared / "include"),
                        str(Path(__file__).with_suffix(".c")), str(shared / "src" / "usb_host_hid.c"), str(shared / "src" / "hid_descriptor.c"), "-o", str(output)],
                       env=env, check=True)
        subprocess.run([str(output)], env=env, check=True)

if __name__ == "__main__":
    test_hid()
