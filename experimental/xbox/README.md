# LibrePadPS5 Xbox Wireless experiment

Native PS5 port experiment for Xbox One controller model **1697** and the original
Xbox Wireless Adapter **045e:02e6**. The slim **045e:02fe** is also allowlisted;
it has not been tested here. This is a separate payload: the released Switch USB
payload and the other agent's checkout are unchanged.

**Status: builds and host tests pass; PS5 pairing/input is not yet verified.**
Do not describe this as hardware-supported until console testing passes.

## What is implemented

- xow's MT76 radio initialization, firmware upload, WLAN association/pairing,
  controller disconnect handling, and GIP acknowledgements.
- A native PS5/FreeBSD USB transport, replacing libusb: vendor control requests,
  bulk endpoints **85 IN**, **84 IN**, and **04 OUT**, persistent buffers and
  device-wide completion dispatch. Firmware chunks are up to `0x3808` bytes.
- A controller sink replacing Linux uinput: Xbox A/B/X/Y become
  Cross/Circle/Square/Triangle; Menu/View become Options/Create; Guide becomes
  PS. Sticks, D-pad, bumpers, stick clicks and analog triggers are mapped.
- One controller in virtual slot zero. The existing protected-user lifecycle,
  process lock and persisted Release/paused state are reused. Additional
  controllers are ignored; they cannot replace the first controller.
- An Xbox-specific dashboard on port **8096**. No Bluetooth, Linux kernel module,
  PC relay, driver replacement on Windows, or generic HID parsing is used.

Rumble, audio, touchpad, motion, custom Xbox remapping, Series-specific features
and multi-controller support are not implemented by this experiment.

## Small integration module

The main project only needs `librepad_xbox.h` (four C functions) and the optional
`dist/liblibrepad_xbox.a` archive. The module exports no `usb_hotplug_*` functions
and does not replace the main project's existing USB manager. Radio/GIP code is
behind the interface and can stay disabled in normal builds.

```c
#include "librepad_xbox.h"

/* After vpad_init(), when Xbox is the selected input backend: */
librepad_xbox_start();
/* In the normal controller loop, alongside vpad_poll(): */
librepad_xbox_poll(now);
/* Dashboard connection status: */
int connected = librepad_xbox_is_connected();
/* Before vpad_cleanup_all(): */
librepad_xbox_stop();
```

This version manages slot zero. Choose either the existing Switch backend or the
Xbox backend for that slot; do not feed both into the same virtual pad. A future
shared device coordinator can arbitrate multiple controller sources. Link with
the SDK's C++ linker, or add its libc++/libc++abi/unwind libraries to the existing
link step; the rest of LibrePadPS5 can remain C.

To build only the optional module:

```sh
python experimental/xbox/build.py --sdk /path/to/ps5-payload-sdk --clang /path/to/clang --module-only
```

`port/standalone.cpp` is only the test payload's USB-manager adapter; exclude it
when linking the module into the main application. The standalone dashboard
guards are likewise not required for module integration.

## Standalone test build

From the repository root, using PS5 Payload SDK v0.43 and LLVM:

```sh
python experimental/xbox/build.py --sdk /path/to/ps5-payload-sdk --clang /path/to/clang
```

The Python builder works on Windows and Unix. A normal SDK/toolchain setup can
also use `make -f experimental/xbox/xbox.mk`.

Outputs: `dist/liblibrepad_xbox.a` and `dist/LibrePadPS5-Xbox-Experimental.elf`. The ELF is a native x86-64 PS5 PIE
payload with C++ statically linked and the usual console service imports.

## Firmware

The adapter requires Microsoft's firmware. **It is not embedded in the ELF or
included in source packages.** Use a local installed driver copy:

```sh
python experimental/xbox/copy_firmware_windows.py --out /path/to/staging/firmware.bin
```

The default source is `C:/Windows/System32/drivers/FW_ACC_00U.bin`. Upload the
staged file through the console's existing FTP service to:

```text
/data/pdp-pad/xbox/firmware.bin
```

The legacy `/data/pdp-pad` namespace is intentional: it shares the process lock
and protected-user configuration with existing builds. Renaming it without a
migration would discard that protection. Keep `native-user`, `pdp-user`, and
`pause` files in place. Do not overwrite or delete the other agent's settings.

The local 2015 firmware inspected here is 70,008 bytes with ILM 60,856 bytes and
DLM 9,120 bytes. SHA-256:
`080ce4091e53a4ef3e5fe29939f51fd91f46d6a88be6d67eb6e99a5723b3a223`.
Length validation prevents truncated/malformed memory images from being uploaded;
it is not a firmware authenticity check. Missing firmware disables radio startup.

## Console test

1. Wait for the other agent's console tests to finish. Stop the existing
   LibrePadPS5/controller payload cleanly. This build uses the same instance lock.
2. Keep the DualSense on its explicitly protected user and sign in a separate
   Xbox user, using the project's existing profile protection setup.
3. Stage the firmware, then move the USB adapter from Windows to the PS5.
4. Load `LibrePadPS5-Xbox-Experimental.elf` using the existing ELF loader.
5. Open `http://<PS5_IP>:8096/`, then Read log. Confirm `radio endpoints opened`
   and `Dongle initialized`. If initialization fails, retain the exact error/log.
6. Press the pairing buttons on the adapter and the 1697 controller. Confirm
   `controller announced 045e:02dd` and controller-connected status.
7. Click Reconnect if paused, then press Guide/PS and select the separate Xbox
   user. Test all buttons, sticks, analog triggers and the existing DualSense.
8. Test Release, explicit Reconnect, controller power-off/on, adapter unplug/replug,
   and Stop. Held input must clear on disconnect and Release must remain paused.

The payload has not been sent to a PS5 during this experiment. Driver behavior,
radio initialization, pairing and in-game input still require that test.

## Verification performed

```sh
python experimental/xbox/test.py --clang /path/to/clang++ --firmware /path/to/firmware.bin --sanitize
python experimental/xbox/probe_windows.py --seconds 5
```

Host tests cover controller mapping, 10-bit trigger conversion, stick extremes,
Guide press/release preservation, ACK messages, accessories, truncated/chunked
packets and 50,000 generated malformed GIP packets; concurrent completion dispatch
across both readers and the writer; full-size firmware transfers, timeouts,
partial writes, constructor-failure cleanup; firmware header bounds; and single-pad,
missing-firmware, persisted pause, disconnect, reconnect and shutdown behavior.

All four host suites also passed with ASan/UBSan on Windows.
The tests use mock USB ioctls and pad services; they do not verify the console's
USB implementation. On Windows they compile with packed GCC bitfield layout,
matching the PS5 target. Unix host runs also enable ASan/UBSan.

The Windows read-only probe detected an input controller with VID/PID 045e:02dd.
Windows PnP reports its parent as the 045e:02e6 adapter, confirming this is the
user's receiver/controller pair. XInput's wireless capability flag was false
despite that parent relationship; the script labels it as a capability flag,
not a reliable connection-type assertion. No Windows USB driver was changed.

## Source provenance

Radio/GIP code: [medusalix/xow](https://github.com/medusalix/xow), commit
`d335d6024f8380f52767a7de67727d9b2f867871`, GPL-2.0-or-later. Copyright and
license text remain in `vendor/xow`; xow's MT76 code also retains the upstream
ISC notices. See [xow's explanation](https://github.com/medusalix/xow#how-it-works)
and [supported controllers](https://github.com/medusalix/xow#supported-devices).

Local changes to that code: replace USB/log/controller interfaces; reject truncated
and chunked GIP messages; validate firmware bounds; avoid stepping iterators past
the firmware image. The protocol/radio core remains recognizably upstream.

[xone](https://github.com/medusalix/xone) was inspected as a reference. Its Linux
kernel interface is unsuitable for direct loading on PS5; no xone code is built.
