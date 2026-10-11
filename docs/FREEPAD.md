# FreePadUSB setup and reference

Use a supported wired USB controller on a jailbroken PS5. USB only; no Bluetooth.

**[Download ELF](https://github.com/arwebSE/freepad-usb-ps5/releases/latest/download/FreePad-USB.elf)** · [Releases](https://github.com/arwebSE/freepad-usb-ps5/releases) · [Issues](https://github.com/arwebSE/freepad-usb-ps5/issues)

## Supported hardware

- **Hardware verified:** PDP Faceoff Deluxe+ Audio Wired Switch controller, USB ID `0e6f:0184`. Development builds also accept configurable HID layouts; other controllers are not physically verified.
- Tested on **PS5 firmware 12.60**: sticks, buttons, D-pad, dashboard and separate-profile assignment. Individual games/emulators and other firmware are not explicitly verified.
- No rumble, motion, controller audio or touchpad movement. Triggers are digital.

## Setup

Requires an existing jailbreak, ELF loader or Payload Manager and FTP access.

**Use a separate PS5 user for the USB controller**, such as `SwitchCtl`. Do not assign it to your DualSense user.

1. Confirm your DualSense works. Stop AnyPad, OmniPad and older controller payloads.
2. Through FTP, create the state directory listed in [upgrade compatibility](#troubleshooting--upgrades) and upload an empty file named `pause` into it.
3. Load `FreePadUSB.elf`, plug in the controller and open **`http://<PS5_IP>:8096/`**.
4. Protect your DualSense user: open `/api/state` at that address, find its eight-digit user `id`, put only that ID in a plain-text file named `native-user`, and upload it into the state directory. Click **Stop payload**, then reload the ELF.
5. Click **Reconnect**, press **Home** or the dashboard's **Press Home / PS**, and select **`SwitchCtl`** in the console's user picker. Check both controllers.

Optional: save the USB controller user's ID in `pdp-user` in the same directory for automatic assignment. Settings are read on startup; restart after editing. Never copy IDs from another console. Invalid or identical protected/preferred IDs prevent startup. Protection does not override the console's own user picker.

Payload Manager: place the ELF and [metadata](../payload/FreePadUSB.elf.json) together in `/data/pldmgr/payloads/FreePadUSB/`. Older releases download under the previous ELF filename; rename the downloaded ELF to `FreePadUSB.elf`. Keep one installed entry and stop the old instance before loading a replacement.

## Dashboard

**`http://<PS5_IP>:8096/`** — trusted LAN only, no authentication. Do not expose publicly.

- **Home / PS:** sends PS through our virtual pad.
- **Assign profile:** selects an eligible signed-in user; protected profiles are rejected.
- **Release:** removes our pad and stays paused across unplugging/reloads.
- **Reconnect:** clears the pause and creates our pad again.
- **Refresh log / Stop payload:** view logs or cleanly exit.

Native connection status **unavailable** does not mean your DualSense is disconnected. Configure explicit profile protection; do not rely on detection alone.

## Buttons

`B → Cross` · `A → Circle` · `Y → Square` · `X → Triangle` (physical positions).

`L/R → L1/R1` · `ZL/ZR → L2/R2` · `−/+ → Create/Options`.

`Home → PS` · `Capture → touchpad click`. Sticks, D-pad and stick clicks map normally.

## Controller profiles & remapping (preview)

Requires the development build, not the older stable download. One controller at a time. Custom layouts are host-tested only; keep the previous release available.

1. Enter the controller’s four-digit hexadecimal VID/PID.
2. Choose `switch8` for the tested layout, or enter report length, report ID, hat and axis byte offsets and click **Use custom layout**. Offsets include any real report-ID byte. Use `-1` to disable a field.
3. Set each input’s source bit and PS5 output. **Default mapping** restores the tested preset, without saving.
4. Click **Add / update in editor**, **Release virtual pad**, then **Save & apply** after removal. Click **Reconnect**; newly added devices may need replugging.

Import/export operates on the editor contents. Up to eight profiles are saved atomically in `controllers.conf` in the state directory. Invalid settings leave the previous configuration unchanged; invalid files refuse startup. A missing file uses the tested default.

`switch8` preserves the original 8-byte format: 14 bits X,A,B,Y,L,R,ZL,ZR,Minus,Plus,L3,R3,Home,Capture; hat byte 2 (0–7, neutral 15); unsigned axes in bytes 3–6.

```text
0e6f:0184 switch8 triangle,circle,cross,square,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad
```

Custom format: `hid:length:reportID:hatByte:neutralHat:lxByte:lyByte:rxByte:ryByte`.
Each of the 14 bindings is `output@absoluteBit`. Bits are least-significant-first in each byte. Report lengths are 1–64 bytes; ID `-1` means no ID. Hat uses the low nibble; neutral must be 8–15. Axes are unsigned 8-bit. Buttons are active-high.

Example only (not a verified controller): 12 bytes, report ID 7, no hat, axes bytes 4–7, Cross at byte 8 bit 0 and D-pad at the following bits:

```text
1234:5678 hid:12:7:-1:15:4:5:6:7 cross@64,up@65,right@66,down@67,left@68,none@69,none@70,none@71,none@72,none@73,none@74,none@75,none@76,none@77
```

Outputs: `none,cross,circle,square,triangle,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad,up,right,down,left`. Digital trigger values follow L2/R2 outputs. No automatic layout detection, signed/16-bit axes or proprietary initialization. Those protocols require a dedicated driver.

**View latest raw report** shows bytes from an explicitly configured device, even while paused. For initial investigation, capture on a PC using `python tools/usb_report_probe.py --vid 1234 --pid 5678` (requires `hidapi`). Do not guess layouts on the console. Sony VID `054c` is blocked; explicit configuration is not proof that a third-party device cannot interfere with native input.

The original protected-user settings and native-device ownership checks still apply. New IDs remain disallowed unless saved in a valid profile. There is no generic-parser fallback.

## Troubleshooting / upgrades

- **Inactive pad:** click Reconnect, check the log and USB ID. If unassigned, press Home and select the separate user. For game focus, try Home/PS; game-specific behavior is not verified.
- **Skipped D-pad taps:** use the current release and stop older instances. Report recurring problems with logs.
- **DualSense cannot connect:** stop FreePadUSB, unplug the USB controller and fully shut down the console—not Rest Mode. A development-session failure required a full reboot; its cause remains unconfirmed. If needed, [reset the DualSense](https://www.playstation.com/en-us/support/hardware/troubleshoot-dualsense/) and reconnect with a USB data cable. Do not update firmware just to troubleshoot.
- **Dashboard offline:** check the console IP, HTTP and port `8096`. It closes when the payload stops.
- **Stop through FTP:** upload an empty file named `stop` into the state directory.
- **Upgrade compatibility:** the on-console state directory is still `/data/pdp-pad/`; keep it and your profile files. The legacy `pdp-user` filename and lock remain compatible with previous builds. These paths are centralized in [legacy_paths.h](../src/core/legacy_paths.h); changing them without migration would lose profile protection and allow old/new instances to use separate locks. Historical releases keep their original assets.

The payload manages its own virtual pad; no Bluetooth scanning, shell-process injection, native-handle fallback or autoload. It still uses elevated credentials and shared controller services: crashes/service issues remain possible. Do not run competing controller payloads.

## Build

Docker with Linux containers, from the repo root:

```sh
docker build -t freepadusb-builder .
docker run --rm -v "$PWD:/work" -w /work freepadusb-builder
```

PowerShell: use `"${PWD}:/work"` for the volume. Output: `dist/FreePadUSB.elf`. Builder uses **PS5 Payload SDK v0.43**. With the SDK installed: `make -f freepad.mk`. Dashboard assets are generated from `web/` during the build; no extra files need uploading.

Host tests (run inside a disposable builder container; they create the legacy state directory and do not contact a PS5):

Run all four sanitizer-enabled suites with `make test`. This also generates the dashboard assets. Use the disposable container, not the live console.

## Credits

Derived from [OmniPad PS5](https://github.com/diegobarbosaa/OmniPad-PS5) by diegobarbosaa. Thanks to [AnyPad](https://github.com/sinfiltros/AnyPad-PS5), [PoorDS4/Ghostcontrol](https://github.com/a-ddr/PoorDS4), [YetAnotherControllerEnabler](https://github.com/MegaCadeDev/YetAnotherControllerEnabler) and [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) contributors. Original source attribution retained; Git history contains this project's commits only.

[GPL-3.0](../LICENSE). Not affiliated with Sony, Nintendo or controller manufacturers.
