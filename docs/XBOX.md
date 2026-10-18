# Xbox Wireless

## Setup

1. Load the [latest LibrePadPS5 ELF](https://github.com/arwebSE/LibrePadPS5/releases/latest/download/LibrePadPS5.elf).
2. Plug the Xbox Wireless Adapter into the PS5. The required firmware is included;
   LibrePadPS5 loads it only after detecting a supported adapter.
3. Press the pairing buttons on the adapter and controller.
4. Press Xbox/Guide and select a separate PS5 profile, or assign one in the
   dashboard at `http://<PS5_IP>:8096/`. Reserve profiles used by native DualSense controllers.

Xbox One model **1697** with adapter **045e:02e6** is tested. The slim
**045e:02fe** adapter is supported by detection but remains untested here.
One Xbox controller can run alongside other USB gamepads using separate profiles.
Multiple Xbox controllers on one adapter, rumble, audio, motion and touchpad
are not implemented.

## Firmware

The bundled [FW_ACC_00U.bin](../vendor/xbox/FW_ACC_00U.bin) runs on the adapter's
radio. It is shared by compatible Xbox Wireless controllers, rather than tied
to model 1697. Normal setup needs no download, script, firmware upload or setup ZIP.
No firmware file is created on the console when using the bundled image.

## Advanced

### Manual firmware override

An existing `/data/LibrePadPS5/xbox/firmware.bin` takes priority over the bundled
image. To return to the bundled image, remove that override through FTP and
restart LibrePadPS5. Unplug and replug the adapter after changing firmware;
restarting the payload can reuse firmware already running on the radio.

To install an override, open **Controller > Xbox Wireless setup > Advanced
firmware override** in the dashboard, select `FW_ACC_00U.bin` and choose
**Install firmware**. Uploads are checked for size and memory layout, then saved
atomically. An invalid or unreadable override stops radio initialization and is
reported in the dashboard and log.

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

For FTP, copy the verified image to `/data/LibrePadPS5/xbox/firmware.bin`.
[Storage paths](STORAGE.md) describes migration from older installations.

### Builds and module integration

The standard build includes Xbox Wireless. `WITH_XBOX=0` omits the backend and
its firmware. On Windows or Unix with LLVM and PS5 Payload SDK v0.43+:

```sh
python tools/xbox/build.py --combined --sdk /path/to/ps5-payload-sdk --clang /path/to/clang
```

Omit `--combined` for the standalone diagnostic payload, or use `--module-only`
for `dist/liblibrepad_xbox.a`. All three builds include the verified firmware.
The module's [C interface](../src/xbox/librepad_xbox.h) uses the shared controller
router and requires the SDK C++ runtime. Start it after virtual-pad initialization,
poll it in the controller loop and stop it before virtual-pad cleanup.

Host regression tests:

```sh
python tests/xbox/run.py --clang /path/to/clang++ --firmware vendor/xbox/FW_ACC_00U.bin --sanitize
```

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
