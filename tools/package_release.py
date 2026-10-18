"""Stage the three stable release assets; manual Xbox helpers stay in tools/xbox."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="Empty staging directory")
    args = parser.parse_args()
    payload = ROOT / "dist/LibrePadPS5.elf"
    metadata_path = ROOT / "config/payload/LibrePadPS5.elf.json"
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    version = metadata["version"]
    data = payload.read_bytes()
    if data[:4] != b"\x7fELF" or ("LibrePadPS5 " + version + ": starting").encode() not in data:
        parser.error("ELF does not match the stable metadata version")
    if (ROOT / "vendor/xbox/FW_ACC_00U.bin").read_bytes() not in data:
        parser.error("Standard release ELF must include the Xbox adapter firmware")
    if metadata["filename"] != payload.name or metadata["url"] != (
            f"https://github.com/arwebSE/LibrePadPS5/releases/download/v{version}/{payload.name}"):
        parser.error("Unexpected release filename or URL")
    output = args.output or ROOT / "dist/releases" / ("v" + version)
    if output.exists() and any(output.iterdir()):
        parser.error("Staging directory must be empty; choose a fresh --output directory")
    output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(payload, output / payload.name)
    shutil.copyfile(metadata_path, output / metadata_path.name)
    digest = hashlib.sha256(data).hexdigest()
    (output / "LibrePadPS5.elf.sha256").write_text(digest + "  LibrePadPS5.elf\n", encoding="ascii")
    print(f"Staged ELF, metadata and checksum in {output}; SHA256 {digest}")


if __name__ == "__main__":
    main()
