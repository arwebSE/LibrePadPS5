"""Build the isolated native Xbox radio payload with LLVM and PS5 SDK v0.43+."""
import argparse
import pathlib
import subprocess
import shutil

ROOT = pathlib.Path(__file__).resolve().parents[2]
C_SOURCES = ["util", "log", "usb_controllers", "freepad_profiles", "shellui_inject", "ps5_vpad", "web_freepad", "main_freepad"]
CPP_SOURCES = ["port/backend.cpp", "port/usb.cpp", "port/controller.cpp", "port/log.cpp", "vendor/xow/dongle/mt76.cpp", "vendor/xow/dongle/dongle.cpp", "vendor/xow/controller/gip.cpp"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", required=True, type=pathlib.Path)
    parser.add_argument("--clang", default="clang", help="Path to clang executable")
    parser.add_argument("--module-only", action="store_true", help="Build the optional static module without a standalone payload")
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
              "-O2", "-Wall", "-Wextra", "-ffunction-sections", "-fdata-sections", "-DFREEPAD_ONLY",
              "-DLIBREPAD_XBOX_EXPERIMENT", "-I", str(ROOT / "src")]
    objects = []
    sources = [] if args.module_only else [(ROOT / "src" / (name + ".c"), False) for name in C_SOURCES]
    sources += [(ROOT / "experimental/xbox" / name, True) for name in CPP_SOURCES]
    for source, cpp in sources:
        obj = build / (source.stem + ("_cpp" if cpp else "_c") + ".o")
        extra = ["-x", "c++", "-std=c++11", "-frtti", "-fexceptions", "-isystem", str(sdk / "target/include/c++/v1"),
                 '-DFIRMWARE="/data/pdp-pad/xbox/firmware.bin"'] if cpp else ["-std=c11"]
        print("Compiling", source.relative_to(ROOT), flush=True)
        subprocess.run([str(compiler), *extra, *common, "-c", str(source), "-o", str(obj)], check=True)
        objects.append(str(obj))
    archive = ROOT / "dist/liblibrepad_xbox.a"
    archive.parent.mkdir(exist_ok=True)
    ar = compiler.with_name("llvm-ar.exe" if compiler.suffix == ".exe" else "llvm-ar")
    subprocess.run([str(ar), "rcs", str(archive), *[obj for obj in objects if obj.endswith("_cpp.o")]], check=True)
    print("Built optional module", archive)
    if args.module_only:
        return
    source = ROOT / "experimental/xbox/port/standalone.cpp"
    glue = build / "standalone_cpp.o"
    subprocess.run([str(compiler), *extra, *common, "-c", str(source), "-o", str(glue)], check=True)
    objects.append(str(glue))
    output = ROOT / "dist/LibrePadPS5-Xbox-Experimental.elf"
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
