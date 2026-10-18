# Xbox Wireless

Microsoft's USB receiver handles Xbox Wireless pairing for the controller.
Leave LibrePadPS5's built-in Bluetooth disabled for this connection. For an
8BitDo receiver, use [Adapter 2 setup](SETUP.md#8bitdo-usb-wireless-adapter-2).

The latest release includes the adapter firmware. For an older ELF requesting
one-time setup, use the [manual install](#advanced).

## Setup

1. Load the [latest LibrePadPS5 ELF](https://github.com/arwebSE/LibrePadPS5/releases/latest/download/LibrePadPS5.elf).
2. Plug the Xbox Wireless Adapter into the PS5. In bundled-firmware builds,
   LibrePadPS5 detects the supported adapter and initializes it automatically.
3. Turn on the controller, press the adapter's pairing button and hold the controller's pairing button until its Xbox light flashes rapidly. Wait for pairing to finish and the light to become steady.
4. Press Xbox/Guide and select a separate PS5 profile, or assign one in the
   dashboard at `http://<PS5_IP>:8096/`. Reserve profiles used by native DualSense controllers.

Xbox One model **1697** with adapter **045e:02e6** is tested. The slim
**045e:02fe** adapter is supported by detection but remains untested here.
One adapter and one Xbox Wireless controller are handled at a time. A second
adapter does not add another supported wireless player. The Xbox can run
alongside PDP and other USB gamepads on separate PS5 profiles; see the
[multiplayer example](SETUP.md#multiple-controllers). Bluetooth mode is a separate
path and requires other controller sources to be disconnected before pairing.

## Button mapping

| Xbox input | PS5 input |
|---|---|
| A / B / X / Y | Cross / Circle / Square / Triangle |
| LB / RB, LT / RT | L1 / R1, L2 / R2 (analog triggers) |
| View / Menu | Create / Options |
| Left / right stick click | L3 / R3 |
| Xbox / Guide | PS |

Sticks and D-pad retain their usual roles. This wireless mapping is fixed;
the dashboard's USB/XInput driver selection does not change it. Touchpad,
motion, rumble, audio and Series-specific extra controls are not implemented.

## Firmware

The bundled [FW_ACC_00U.bin](../vendor/xbox/FW_ACC_00U.bin) runs on the adapter's
radio. It is shared by compatible Xbox Wireless controllers, rather than tied
to model 1697. Normal setup needs no download, script, firmware upload or setup ZIP.
No firmware file is created on the console when using the bundled image, and
no internet connection is needed for that path. Wired Xbox controllers,
8BitDo USB adapters and built-in Bluetooth connections do not use this image.

Firmware transfers start only after detecting adapter `045e:02e6` or
`045e:02fe`. Other controllers do not trigger a firmware load. A resident image
on a warm adapter can be restarted without uploading it again; unplugging the
adapter clears that state.

LibrePadPS5 remembers the last successfully connected controller and its identity in
`/data/LibrePadPS5/xbox-controller` and advertise for it after a service restart
or disconnect. Leave the adapter plugged in and turn the controller on. The
adapter's pairing button still allows replacing it with a different controller.
Its saved identity also restores input if a controller stays powered on through
the restart and resumes input without repeating its identity announcement.

The dashboard shows **Firmware included** when using the bundle, or **Firmware
installed** when a saved console copy takes priority. Both are ready states;
firmware readiness alone does not mean a controller is paired or assigned to
a PS5 user.

## Troubleshooting

- **One-time firmware setup needed:** this is an older ELF without the bundle. Use the manual install below, or build the current source.
- **Invalid/unreadable firmware:** a saved override blocks initialization. Replace it with the verified image or follow the steps below to return to the bundle.
- **Xbox light keeps flashing:** turn the controller on and repeat pairing. After changing firmware, unplug/replug the adapter before trying again.
- **Controller connected but no console input:** select the Xbox in the dashboard, reconnect if paused, assign its own signed-in PS5 user and press Xbox/Guide. Check whether the displayed sticks/buttons change.
- **Adapter initialization keeps failing:** download a diagnostic report under **Advanced > Logs & support**. Include the adapter VID/PID and controller model; a missing paired controller and a USB transport failure need different fixes.

## Advanced

### Manual firmware override

An existing `/data/LibrePadPS5/xbox/firmware.bin` takes priority over the bundled
image. To return to the bundled image, stop LibrePadPS5, back up and remove only
that override through FTP, then reload a bundled-firmware build. Unplug and
replug the adapter after changing firmware; restarting the payload can reuse
firmware already running on the radio.

To install an override, open **Controller > Xbox Wireless setup > Advanced
firmware override** in the dashboard, select `FW_ACC_00U.bin` and choose
**Install firmware**. Uploads are checked for size and memory layout, then saved
atomically. An invalid or unreadable override stops radio initialization and is
reported in the dashboard and log.
Older dashboards may show the upload picker directly or under **Replace adapter
firmware** instead.

You can use the [provided firmware file](../vendor/xbox/FW_ACC_00U.bin) as the
override too. Manual uploads validate size and memory layout; they do not
compare the known hash. Verify the file as below, or let the Windows helper
perform that check.

### Download and verify

The supplied image is build `201703281033____`, 70,620 bytes. Its SHA-256 is:

```text
48084d9fa53b9bb04358f3bb127b7495dc8f7bb0b3ca1437bd24ef2b6eabdf66
```

To obtain it independently, download the [Microsoft driver package](http://download.windowsupdate.com/c/msdownload/update/driver/drvs/2017/07/1cd6a87c-623f-4407-a52d-c31be49e925c_e19f60808bdcbfbd3c3df6be3e71ffc52e43261e.cab),
extract `FW_ACC_00U.bin` and compare its hash:

```powershell
Get-FileHash ./FW_ACC_00U.bin -Algorithm SHA256
```

### Quick install on Windows

The optional [setup.cmd](../tools/xbox/setup.cmd) and
[setup.ps1](../tools/xbox/setup.ps1) helpers stay in `tools/xbox/` in the source
repository. They are not release assets. Keep them together and run `setup.cmd`,
or run from a checkout:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/xbox/setup.ps1 -PS5Address 192.168.1.100
```

Replace the example IP with your console's address. The helper downloads from
Microsoft, verifies the hash and uploads an override through the running dashboard.
Use `-FirmwarePath ./FW_ACC_00U.bin` to supply a local image instead.
The helper needs the running dashboard. Supplying a local image skips the
Microsoft download.

For FTP, copy the verified image to `/data/LibrePadPS5/xbox/firmware.bin`.
[Storage paths](STORAGE.md) describes migration from older installations.

### Build or customize

See [development](DEVELOPMENT.md) for combined, standalone and module-only
builds, firmware embedding, tests and packaging. `WITH_XBOX=0` omits both the
backend and its firmware.

## Source provenance

Radio/GIP code: [medusalix/xow](https://github.com/medusalix/xow), commit
`d335d6024f8380f52767a7de67727d9b2f867871`, GPL-2.0-or-later. Copyright and
license text remain in [vendor/xow](../vendor/xow/); xow's MT76 code also retains the upstream
ISC notices. See [xow's explanation](https://github.com/medusalix/xow#how-it-works)
and [supported controllers](https://github.com/medusalix/xow#supported-devices).

Local changes to that code: replace USB/log/controller interfaces; reject truncated
and chunked GIP messages; validate firmware bounds; avoid stepping iterators past
the firmware image; support reassociation and deduplicate radio clients; skip
channel-scan responses; restart resident firmware; reconnect the saved controller
without pressing the adapter button. The protocol/radio core
remains recognizably upstream.

[xone](https://github.com/medusalix/xone) was inspected as a reference. Its Linux
kernel interface is unsuitable for direct loading on PS5; no xone code is built.
