"""Read-only XInput probe. Uses the installed Windows adapter driver; no USB writes."""
import argparse
import ctypes as c
import json
import time


class Gamepad(c.Structure):
    _fields_ = [("buttons", c.c_uint16), ("lt", c.c_uint8), ("rt", c.c_uint8),
                ("lx", c.c_int16), ("ly", c.c_int16), ("rx", c.c_int16), ("ry", c.c_int16)]


class State(c.Structure):
    _fields_ = [("packet", c.c_uint32), ("gamepad", Gamepad)]


class Vibration(c.Structure):
    _fields_ = [("left", c.c_uint16), ("right", c.c_uint16)]


class Capabilities(c.Structure):
    _fields_ = [("type", c.c_uint8), ("subtype", c.c_uint8), ("flags", c.c_uint16),
                ("gamepad", Gamepad), ("vibration", Vibration)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=float, default=5)
    args = parser.parse_args()
    if not 0 <= args.seconds <= 300:
        parser.error("seconds must be between 0 and 300")
    dll = c.WinDLL("xinput1_4.dll")
    get_state = dll.XInputGetState
    try:
        get_state = dll[100]  # XInputGetStateEx also exposes Guide; fallback is public API.
    except AttributeError:
        pass
    get_state.argtypes = [c.c_uint32, c.POINTER(State)]
    get_state.restype = c.c_uint32
    dll.XInputGetCapabilities.argtypes = [c.c_uint32, c.c_uint32, c.POINTER(Capabilities)]
    dll.XInputGetCapabilities.restype = c.c_uint32
    previous = {}
    deadline = time.monotonic() + args.seconds
    while True:
        for slot in range(4):
            state = State()
            result = get_state(slot, c.byref(state))
            if result == 1167:
                snapshot = {"slot": slot, "connected": False}
            elif result:
                snapshot = {"slot": slot, "error": result}
            else:
                caps = Capabilities()
                status = dll.XInputGetCapabilities(slot, 0, c.byref(caps))
                pad = state.gamepad
                snapshot = {"slot": slot, "connected": True, "wireless_capability_flag": bool(caps.flags & 2) if not status else None,
                            "packet": state.packet, "buttons": f"{pad.buttons:04x}",
                            "triggers": [pad.lt, pad.rt], "sticks": [pad.lx, pad.ly, pad.rx, pad.ry]}
            if previous.get(slot) != snapshot:
                print(json.dumps(snapshot), flush=True)
                previous[slot] = snapshot
        if time.monotonic() >= deadline:
            break
        time.sleep(0.004)


if __name__ == "__main__":
    main()
