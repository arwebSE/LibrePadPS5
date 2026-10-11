"""Build LibrePadPS5 and its native Xbox Wireless module with LLVM and PS5 SDK v0.43+."""
import argparse
import pathlib
import subprocess
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
C_SOURCES = ["src/core/time_utils.c", "src/core/storage.c", "src/core/logging.c", "src/input/protocol_decoder.c",
             "src/input/controller_profiles.c", "src/input/controller_router.c", "src/ps5/system_bridge.c", "src/ps5/virtual_controller.c",
             "src/dashboard/http_server.c", "src/app/main.c", "src/xbox/standalone_bluetooth.c"]
CPP_SOURCES = ["src/xbox/backend.cpp", "src/xbox/usb.cpp", "src/xbox/controller.cpp", "src/xbox/log.cpp", "vendor/xow/dongle/mt76.cpp", "vendor/xow/dongle/dongle.cpp", "vendor/xow/controller/gip.cpp"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", required=True, type=pathlib.Path)
    parser.add_argument("--clang", default="clang", help="Path to clang executable")
    parser.add_argument("--module-only", action="store_true", help="Build the optional static module without a standalone payload")
    parser.add_argument("--combined", action="store_true", help="Build USB, opt-in Bluetooth and Xbox Wireless together")
    parser.add_argument("--radio-debug", action="store_true", help="Log firmware and radio calibration diagnostics")
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    clang = shutil.which(args.clang)
    if not clang:
        parser.error("clang executable not found")
    compiler = pathlib.Path(clang).resolve()
    linker = compiler.with_name("ld.lld.exe" if compiler.suffix == ".exe" else "ld.lld")
    if not linker.exists() or not (sdk / "target/lib/crt1.o").exists():
        parser.error("LLVM linker or PS5 SDK CRT missing")
    build = ROOT / "build/xbox"
    build.mkdir(parents=True, exist_ok=True)
    common = ["-target", "x86_64-sie-ps5", "-isysroot", str(sdk), "-isystem", str(sdk / "target/include"),
              "-fno-stack-protector", "-fno-plt", "-femulated-tls", "-fvisibility-nodllstorageclass=default",
              "-O2", "-Wall", "-Wextra", "-ffunction-sections", "-fdata-sections",
              "-DLIBREPAD_WITH_XBOX" if args.combined else "-DLIBREPAD_XBOX_STANDALONE", "-D_DEFAULT_SOURCE", "-I", str(ROOT / "src"),
              "-I", str(ROOT / "vendor/anypad"),
              "-I", str(ROOT / "vendor"),
              "-I", str(ROOT / "build/generated")]
    objects = []
    if not args.module_only:
        subprocess.run([sys.executable, str(ROOT / "tools/embed_dashboard.py")], check=True)
    sources = [] if args.module_only else [(ROOT / name, False) for name in C_SOURCES]
    if args.combined and not args.module_only:
        sources = [(path, cpp) for path, cpp in sources if path.name != "standalone_bluetooth.c"]
        sources += [(ROOT / name, False) for name in ["src/input/usb_device.c", "src/bluetooth/controller_host.c"]]
        sources += [(ROOT / "vendor/anypad" / (name + ".c"), False) for name in
                    ["util", "crc32", "smp_crypto", "profiles", "generic", "host", "le", "hci_usb", "usb_desc", "evstream"]]
    sources += [(ROOT / name, True) for name in CPP_SOURCES]
    for source, cpp in sources:
        obj = build / (source.relative_to(ROOT).as_posix().replace("/", "_") + ".o")
        extra = ["-x", "c++", "-std=c++11", "-frtti", "-fexceptions", "-isystem", str(sdk / "target/include/c++/v1"),
                 "-include", "core/paths.h", "-DFIRMWARE=LIBREPAD_XBOX_FIRMWARE_PATH"] if cpp else ["-std=c11"]
        if cpp and args.radio_debug:
            extra += ["-DDEBUG"]
        print("Compiling", source.relative_to(ROOT), flush=True)
        subprocess.run([str(compiler), *extra, *common, "-c", str(source), "-o", str(obj)], check=True)
        objects.append(str(obj))
    archive = ROOT / "dist/liblibrepad_xbox.a"
    archive.parent.mkdir(exist_ok=True)
    ar = compiler.with_name("llvm-ar.exe" if compiler.suffix == ".exe" else "llvm-ar")
    archive.unlink(missing_ok=True)
    subprocess.run([str(ar), "rcs", str(archive), *[obj for obj in objects if obj.endswith(".cpp.o")]], check=True)
    shutil.copyfile(ROOT / "src/xbox/librepad_xbox.h", ROOT / "dist/librepad_xbox.h")
    print("Built optional module", archive)
    if args.module_only:
        return
    if not args.combined:
        source = ROOT / "src/xbox/standalone.cpp"
        glue = build / "standalone_cpp.o"
        subprocess.run([str(compiler), *extra, *common, "-c", str(source), "-o", str(glue)], check=True)
        objects.append(str(glue))
    output = ROOT / ("dist/LibrePadPS5.elf" if args.combined else "dist/LibrePadPS5-Xbox.elf")
    output.parent.mkdir(exist_ok=True)
    subprocess.run([str(linker), "-m", "elf_x86_64", "-pie", "-T", str(sdk / "ldscripts/elf_x86_64.x"),
                    "--eh-frame-hdr", "-z", "max-page-size=0x4000", "-mllvm", "-emulated-tls", "--hash-style=gnu", "--gc-sections",
                    "-L", str(sdk / "target/lib"), "-L", str(sdk / "target/user/homebrew/lib"),
                    str(sdk / "target/lib/crt1.o"), *objects, "--start-group", "-lc++", "-lc++abi", "-lunwind", "-lc",
                    "-lkernel_web", "-lSceLibcInternal", "-lSceNet", "-lScePad", "-lSceUserService",
                    "-lSceSystemService", "-lSceAppInstUtil", "--end-group", "-o", str(output)], check=True)
    print("Built", output)


if __name__ == "__main__":
    main()
