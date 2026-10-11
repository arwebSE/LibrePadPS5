# LibrePadPS5 Xbox Wireless

Native PS5 support for Xbox One controller model **1697** and the original
Xbox Wireless Adapter **045e:02e6**. The slim **045e:02fe** is also allowlisted;
it has not been tested here. The optional module integrates with the main payload
through a small C interface and a shared controller router. A separate Xbox-only
payload is retained for radio diagnostics.

**Status: model 1697 paired through adapter 045e:02e6 on PS5 firmware 12.60.
The user confirmed working controller input and PS5 navigation.** Five Xbox host
test suites and the shared-router suite pass with ASan/UBSan. This is still an
optional Xbox Wireless backend.

## What is implemented

- xow's MT76 radio initialization, firmware upload, WLAN association/pairing,
  controller disconnect handling, and GIP acknowledgements. Reassociation
  requests receive the corresponding response, and repeated requests retain
  the same radio client ID and controller owner.
- A native PS5/FreeBSD USB transport, replacing libusb: vendor control requests,
  bulk endpoints **85 IN**, **84 IN**, and **04 OUT**, persistent buffers and
  device-wide completion dispatch. Firmware chunks are up to `0x3808` bytes.
- A controller sink replacing Linux uinput: Xbox A/B/X/Y become
  Cross/Circle/Square/Triangle; Menu/View become Options/Create; Guide becomes
  PS. Sticks, D-pad, bumpers, stick clicks and analog triggers are mapped.
- One Xbox controller with a dynamically allocated virtual slot. The existing protected-user lifecycle,
  process lock and persisted Release/paused state are reused. Additional
  controllers are ignored; they cannot replace the first controller.
- The current LibrePadPS5 dashboard on port **8096**. The standalone Xbox payload
  disables Bluetooth commands to keep slot zero exclusive to the Xbox radio;
  the module archive exports no Bluetooth symbols. No Linux kernel module,
  PC relay, driver replacement on Windows, or generic HID parsing is used.

Rumble, audio, touchpad, motion, custom Xbox remapping, Series-specific features
and multiple Xbox controllers on one adapter are not implemented by this backend.

## Small integration module

The main project only needs `librepad_xbox.h` (five C functions) and the optional
`dist/liblibrepad_xbox.a` archive. The module exports no `usb_hotplug_*` functions
and does not replace the main project's existing USB manager. Radio/GIP code is
behind the interface and can stay disabled in normal builds.

```c
#include "xbox/librepad_xbox.h"

/* After vpad_init(), alongside the USB backend: */
librepad_xbox_start();
/* In the normal controller loop, alongside USB and vpad_poll(): */
librepad_xbox_poll(now);
/* Dashboard connection status: */
int connected = librepad_xbox_is_connected();
/* Before vpad_cleanup_all(): */
librepad_xbox_stop();
```

The module uses `src/input/controller_router.c` to reserve a separate slot from
USB controllers. Link that shared router along with the main virtual-pad layer.
PDP and Xbox can run together; a released controller retains its own slot. Link with
the SDK's C++ linker, or add its libc++/libc++abi/unwind libraries to the existing
link step; the rest of LibrePadPS5 can remain C.

To build only the optional module:

```sh
python tools/xbox/build.py --sdk /path/to/ps5-payload-sdk --clang /path/to/clang --module-only
```

`src/xbox/standalone.cpp` is only the test payload's USB-manager adapter; exclude it
when linking the module into the main application. Also exclude
`src/xbox/standalone_bluetooth.c`: normal builds retain their existing Bluetooth
implementation. The startup/reconnect guards are only for the standalone build.

## Combined main payload

The standard `make` build includes the optional radio archive by default.
Use `WITH_XBOX=0` for a USB/Bluetooth-only build. On Windows, the portable builder
also builds the standard combined output:

```sh
python tools/xbox/build.py --combined --sdk /path/to/ps5-payload-sdk --clang /path/to/clang
```

This produces `dist/LibrePadPS5.elf`, preserving normal USB drivers and opt-in
Bluetooth. USB and Xbox use different shared slots; Bluetooth pairing still
requires other gamepads to be disconnected. Reserve each native DualSense user
and assign each virtual gamepad to a separate profile in the dashboard.

## Standalone test build

From the repository root, using PS5 Payload SDK v0.43 and LLVM:

```sh
python tools/xbox/build.py --sdk /path/to/ps5-payload-sdk --clang /path/to/clang
```

The Python builder works on Windows and Unix. A normal SDK/toolchain setup can
also use `make xbox`.
Add `--radio-debug` to the Python command for bounded packet diagnostics when
investigating pairing. Normal builds omit those packet dumps.

Outputs: `dist/liblibrepad_xbox.a` and `dist/LibrePadPS5-Xbox.elf`. The ELF is a native x86-64 PS5 PIE
payload with C++ statically linked and the usual console service imports.

## Firmware

The adapter requires Microsoft's firmware. It is downloaded directly from
Microsoft and saved once on the console, outside the ELF, so payload upgrades
keep it. **v1.1.0 adds direct installation; v1.0.0 needs the manual FTP
method below.** The hardware test used the 2017 firmware image
expected by xow: build `201703281033____`, 70,620 bytes, ILM 61,456 bytes and
DLM 9,132 bytes. SHA-256:
`48084d9fa53b9bb04358f3bb127b7495dc8f7bb0b3ca1437bd24ef2b6eabdf66`.

### Quick install on Windows

1. Load the [v1.1.1 main ELF](https://github.com/arwebSE/LibrePadPS5/releases/tag/v1.1.1) and confirm `http://<PS5_IP>:8096/` opens.
2. Download [LibrePadPS5-Xbox-Setup.zip](https://github.com/arwebSE/LibrePadPS5/releases/download/v1.1.1/LibrePadPS5-Xbox-Setup.zip), extract it, and double-click `setup.cmd`.
3. Enter the PS5 IP address. The helper downloads Microsoft's driver package,
   extracts the tested image using Windows' built-in extractor, verifies the
   SHA-256 above and uploads it directly to the dashboard. Python, FTP and a
   separate Xbox payload are unnecessary.
4. Unplug and replug the Xbox adapter, then press the pairing buttons on the
   adapter and Xbox controller. Select the gamepad in the dashboard and assign
   a separate console profile.

From a source checkout, the same helper is [tools/xbox/setup.ps1](../tools/xbox/setup.ps1):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/xbox/setup.ps1 -PS5Address 192.168.1.100
```

Replace the example IP with your console's address. Optional `-FirmwarePath`
installs a local copy instead of downloading; it must match the tested hash.
The helper requires v1.1.0's `/api/xbox` and `/api/xbox/firmware` endpoints.
Its traffic goes to Microsoft for the driver download and to the dashboard for
installation. Firmware remains separate from the release download.

### Upload an existing file from any browser

Open **Controller / Xbox Wireless setup**, select `FW_ACC_00U.bin` or
`firmware.bin`, then **Install firmware**. The dashboard shows missing, invalid
or installed firmware and Xbox controller connection status. Installation saves
the file atomically and queues a radio restart without restarting USB controllers
or the payload. Unplug/replug the Xbox adapter afterward to clear resident firmware.
You can replace an installed file under **Replace adapter firmware**.

Malformed, oversized and incomplete uploads leave the installed file unchanged.
The dashboard validates memory lengths; use the Windows helper for verification
against the known SHA-256. Builds with `WITH_XBOX=0` disable firmware installation.

### Manual installation for v1.0.0 and older builds

Use a local driver copy with the tested version, or obtain the corresponding
driver package from [Microsoft's driver distribution](http://download.windowsupdate.com/c/msdownload/update/driver/drvs/2017/07/1cd6a87c-623f-4407-a52d-c31be49e925c_e19f60808bdcbfbd3c3df6be3e71ffc52e43261e.cab)
and extract `FW_ACC_00U.bin`. The original staging helper accepts an explicit source:

```sh
python tools/xbox/copy_firmware_windows.py --source /path/to/FW_ACC_00U.bin --out /path/to/staging/firmware.bin
```

Without `--source`, the helper reads `C:/Windows/System32/drivers/FW_ACC_00U.bin`.
Check its printed hash: installed Windows drivers may contain an older image.
Upload the staged file through the console's existing FTP service to:

```text
/data/LibrePadPS5/xbox/firmware.bin
```

v1.1.1 automatically migrates existing firmware and settings to the destination
above. v1.1.1-preview and earlier use `/data/pdp-pad/xbox/firmware.bin`.
See [storage paths and migration](STORAGE.md).

The installed Windows copy here was a 2015 image (70,008 bytes); it initialized
the radio but did not pair this controller. A full adapter power cycle was used
after switching to the verified 2017 image. Warm startup reuses resident radio
firmware; changing the file alone does not replace that resident image.
Length validation prevents truncated/malformed memory images from being uploaded;
it is not a firmware authenticity check. Missing firmware disables radio startup.

## Console test

1. Wait for the other agent's console tests to finish. Stop the existing
   LibrePadPS5/controller payload cleanly. This build uses the same instance lock.
2. Keep the DualSense on its explicitly protected user and sign in a separate
   Xbox user, using the project's existing profile protection setup.
3. Load the main `LibrePadPS5.elf` using the existing ELF loader and install the
   firmware from the dashboard or Windows helper. The standalone Xbox ELF is
   only needed when specifically diagnosing radio behavior.
4. Unplug/replug the USB adapter on the PS5, then pair the controller.
5. Open `http://<PS5_IP>:8096/`, then Read log. Confirm `radio endpoints opened`
   and `Dongle initialized`. If initialization fails, retain the exact error/log.
6. Press the pairing buttons on the adapter and the 1697 controller. Confirm
   `controller announced 045e:02dd` and controller-connected status.
7. Click Reconnect if paused, then press Guide/PS and select the separate Xbox
   user. Test all buttons, sticks, analog triggers and the existing DualSense.
8. Test Release, explicit Reconnect, controller power-off/on, adapter unplug/replug,
   and Stop. Held input must clear on disconnect and Release must remain paused.

The rebased payload was loaded on the user's PS5 (firmware `0x12600006`).
The previous PDP process stopped through its existing stop flag, then the new
payload acquired the instance lock, opened the radio endpoints, read wireless
address `62:45:b4:e7:24:99`, and reported `Dongle initialized`. The firmware was
uploaded separately and verified by reading it back. Protected/preferred user
settings were retained. After installing the 2017 firmware and fixing WLAN
reassociation, the controller announced `045e:02dd`, delivered stick/button
reports, and the user confirmed PS5 navigation after pressing Guide and choosing
the separate Xbox profile. In-game behavior, rumble and accessories were not
verified. The transport resets USB before claiming endpoints and permits final
radio-off writes after reader shutdown, allowing subsequent starts to recover.

## Verification performed

```sh
python tests/xbox/run.py --clang /path/to/clang++ --firmware /path/to/firmware.bin --sanitize
python tools/xbox/probe_windows.py --seconds 5
```

Host tests cover controller mapping, 10-bit trigger conversion, stick extremes,
Guide press/release preservation, ACK messages, accessories, truncated/chunked
packets and 50,000 generated malformed GIP packets; concurrent completion dispatch
across both readers and the writer; full-size firmware transfers, timeouts,
partial writes, constructor-failure cleanup; firmware header bounds; and single-pad,
missing-firmware, persisted pause, disconnect, reconnect and shutdown behavior.

The fifth suite exercises MT76/Dongle/GIP together: pairing responses, association
and reassociation subtypes, repeated requests retaining one controller, client
removal/reuse, malformed radio packets and warm startup without DMA re-upload.
All five host suites passed with ASan/UBSan on Windows.
The shared-router suite additionally checks concurrent PDP/Xbox input isolation,
released-slot ownership, capacity, targeted reconnect and persisted pause migration.
Lifecycle, dashboard and Bluetooth adapter regression suites passed using mock
pad/HCI APIs in disposable PS5 SDK executables with a separate test state directory.
The combined payload was built warning-free and its UTF-8 dashboard assets were
compared byte-for-byte against the live PS5 responses.
On PS5 12.60, PDP and Xbox were active simultaneously with distinct profile
assignments. Releasing/reconnecting PDP preserved the Xbox's live status,
profile and advancing injection counter. Reserved/duplicate profile assignments
and malformed slot parameters were rejected. The user reported both pads working
after the combined restart. A full two-DualSense plus PDP/Xbox game test remains
unverified; the console login list exposed only three profiles during this test.
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
license text remain in [vendor/xow](../vendor/xow/); xow's MT76 code also retains the upstream
ISC notices. See [xow's explanation](https://github.com/medusalix/xow#how-it-works)
and [supported controllers](https://github.com/medusalix/xow#supported-devices).

Local changes to that code: replace USB/log/controller interfaces; reject truncated
and chunked GIP messages; validate firmware bounds; avoid stepping iterators past
the firmware image; support reassociation and deduplicate radio clients; skip
channel-scan responses; reuse firmware after warm reset. The protocol/radio core
remains recognizably upstream.

[xone](https://github.com/medusalix/xone) was inspected as a reference. Its Linux
kernel interface is unsuitable for direct loading on PS5; no xone code is built.
