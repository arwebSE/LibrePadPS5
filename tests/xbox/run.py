"""Run native-host tests for the Xbox Wireless transport and controller sink."""
import argparse
import pathlib
import shutil
import subprocess
import sys
import os

ROOT = pathlib.Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang", default="clang++")
    parser.add_argument("--firmware", type=pathlib.Path)
    parser.add_argument("--sanitize", action="store_true", help="Enable ASan/UBSan (requires compiler sanitizer runtime)")
    args = parser.parse_args()
    compiler = shutil.which(args.clang)
    if not compiler:
        parser.error("C++ compiler not found")
    build = ROOT / "build/xbox/tests"
    build.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c++11", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "src"), "-I", str(ROOT / "vendor"), "-I", str(ROOT / "tests")]
    if sys.platform == "win32":
        flags += ["-mno-ms-bitfields"]
        if not args.sanitize:
            flags += ["-static"]
    else:
        flags += ["-pthread"]
    if args.sanitize or sys.platform != "win32":
        flags += ["-fsanitize=address,undefined", "-g"]
    environment = dict(os.environ)
    environment["PATH"] = str(pathlib.Path(compiler).resolve().parent) + os.pathsep + environment.get("PATH", "")
    cases = [
        ("controller", ["tests/xbox/test_controller.cpp", "src/xbox/controller.cpp", "src/xbox/log.cpp", "vendor/xow/controller/gip.cpp"], []),
        ("usb", ["tests/xbox/test_usb.cpp", "src/xbox/usb.cpp"], ["-DXBOX_USB_TEST"]),
        ("firmware", ["tests/xbox/test_firmware.cpp"], []),
        ("backend", ["tests/xbox/test_backend.cpp", "src/xbox/backend.cpp"], ["-DXBOX_BACKEND_TEST", '-DFIRMWARE="test-firmware"']),
        ("radio", ["tests/xbox/test_radio.cpp", "vendor/xow/dongle/mt76.cpp", "vendor/xow/dongle/dongle.cpp",
                   "src/xbox/controller.cpp", "vendor/xow/controller/gip.cpp", "src/xbox/log.cpp"],
         ["-DXBOX_USB_TEST", "-DXBOX_RADIO_TEST", '-DFIRMWARE="build/xbox/tests/radio-firmware.bin"']),
    ]
    for name, sources, defines in cases:
        exe = build / (name + (".exe" if sys.platform == "win32" else ""))
        subprocess.run([compiler, *flags, *defines, *[str(ROOT / source) for source in sources], "-o", str(exe)], check=True, cwd=ROOT)
        extra = [str(args.firmware.resolve())] if name == "firmware" and args.firmware else []
        subprocess.run([str(exe), *extra], check=True, timeout=20, env=environment, cwd=ROOT)


if __name__ == "__main__":
    main()
