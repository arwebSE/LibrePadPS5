"""Read changed input reports from the supported USB 0e6f:0184 controller."""

import argparse
import time
import hid


VID = 0x0E6F
PID = 0x0184


def main():
    parser = argparse.ArgumentParser(description="Capture input reports on a PC; no controller writes.")
    parser.add_argument("--vid", type=lambda value: int(value, 16), default=VID)
    parser.add_argument("--pid", type=lambda value: int(value, 16), default=PID)
    parser.add_argument("--seconds", type=int, default=45)
    args = parser.parse_args()
    if not (0 < args.vid <= 0xFFFF and 0 < args.pid <= 0xFFFF and 1 <= args.seconds <= 300):
        parser.error("VID/PID must be nonzero 16-bit hexadecimal IDs; seconds must be 1–300")
    devices = hid.enumerate(args.vid, args.pid)
    for device in devices:
        print({key: device.get(key) for key in ("path", "product_string", "usage_page", "usage", "interface_number")}, flush=True)
    if not devices:
        raise SystemExit("Controller not detected")

    device = hid.device()
    device.open_path(devices[0]["path"])
    device.set_nonblocking(True)
    print(f"Capturing changed reports for {args.seconds} seconds. Press buttons and move sticks now.", flush=True)
    last = None
    end = time.monotonic() + args.seconds
    try:
        while time.monotonic() < end:
            report = bytes(device.read(64))
            if report and report != last:
                print(f"{time.monotonic():.3f} {report.hex(' ')}", flush=True)
                last = report
            time.sleep(0.005)
    finally:
        device.close()


if __name__ == "__main__":
    main()
