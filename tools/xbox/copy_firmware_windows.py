"""Copy the user's installed Xbox radio firmware into a local staging directory."""
import argparse
import hashlib
import os
import pathlib
import shutil
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=pathlib.Path, default=pathlib.Path(os.environ.get("WINDIR", "C:/Windows")) / "System32/drivers/FW_ACC_00U.bin")
    parser.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args()
    data = args.source.read_bytes()
    if not 32 <= len(data) <= 1024 * 1024:
        parser.error("invalid firmware size")
    ilm, dlm = struct.unpack_from("<II", data)
    if not ilm or not dlm or ilm + dlm + 32 != len(data):
        parser.error("invalid firmware memory lengths")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    if args.out.exists():
        parser.error("destination already exists; choose a new output path")
    shutil.copyfile(args.source, args.out)
    print("Local firmware staged:", args.out.resolve())
    print("Bytes:", len(data), "SHA256:", hashlib.sha256(data).hexdigest())
    print("Published v1.1.1-preview and earlier use /data/pdp-pad/xbox/firmware.bin")
    print("Console destination for v1.1.1 and newer: /data/LibrePadPS5/xbox/firmware.bin")


if __name__ == "__main__":
    main()
