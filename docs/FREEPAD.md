# LibrePadPS5 setup and reference

Use a wired USB, Xbox Wireless or Bluetooth gamepad on a jailbroken PS5. See [tested hardware and controller coverage](CONTROLLERS.md).

**[Download v0.5.0 ELF](https://github.com/arwebSE/freepad-usb-ps5/releases/download/v0.5.0-preview/LibrePadPS5.elf)** · [Releases](https://github.com/arwebSE/freepad-usb-ps5/releases) · [Issues](https://github.com/arwebSE/freepad-usb-ps5/issues)

## Supported hardware

- **Hardware verified:** PDP Faceoff Deluxe+ Audio Wired Switch controller, USB ID `0e6f:0184`. Development builds also accept configurable HID layouts; Xbox One model 1697 with Xbox Wireless Adapter `045e:02e6` is also verified individually on firmware 12.60; the adapter needs separately staged firmware.
- Tested on **PS5 firmware 12.60**: sticks, buttons, D-pad, dashboard and separate-profile assignment. Individual games/emulators and other firmware are not explicitly verified.
- No game-driven rumble, motion forwarding, controller audio or touchpad movement. Dedicated drivers preserve analog triggers where available; the 8-byte preset has digital triggers.

## Setup

Requires an existing jailbreak, ELF loader or Payload Manager and FTP access.

**Use a separate PS5 user for the gamepad**, such as `Gamepad`. Do not assign it to your DualSense user.

1. Confirm your DualSense works. Stop AnyPad, OmniPad and older controller payloads.
2. Through FTP, create the state directory listed in [upgrade compatibility](#troubleshooting--upgrades) and upload an empty file named `pause` into it.
3. Load `LibrePadPS5.elf`, plug in the controller and open **`http://<PS5_IP>:8096/`**.
4. Protect your DualSense user: open `/api/state` at that address, find its eight-digit user `id`, put only that ID in a plain-text file named `native-user`, and upload it into the state directory. Click **Stop payload**, then reload the ELF.
5. Click **Reconnect**, press **Home** or the dashboard's **Press Home / PS**, and select **`Gamepad`** in the console's user picker. Check both controllers.

Optional: save the gamepad user's ID in `pdp-user` in the same directory for automatic assignment. Settings are read on startup; restart after editing. Never copy IDs from another console. Invalid or identical protected/preferred IDs prevent startup. Protection does not override the console's own user picker.

### Multiple controllers

USB and Xbox Wireless share three virtual slots. Native DualSense controllers
remain native and do not consume these slots. For two DualSense controllers plus
PDP and Xbox, use four separate signed-in profiles:

1. Reserve both native profiles in **Advanced / Input & native controllers**.
   The original `native-user` reservation remains protected. Additional IDs are
   saved atomically, one hexadecimal ID per line, in `native-users`.
2. Connect the PDP and Xbox receiver. Stage the Xbox firmware as described in
   [the radio module guide](../modules/xbox/README.md#firmware).
3. Select each pad in **Control gamepad** and **Assign user** to its own signed-in
   profile. A profile already assigned to another virtual pad is refused.
4. Join a game with local multiplayer support and verify each player's input.

**Release virtual pad** and **Reconnect** affect the selected pad. A released
physical device retains its slot, preventing another transport from taking it.
Release persists across reloads/replugging using `pause-N` files.
**Release all virtual pads** also preserves the legacy `pause` file; reconnecting
one pad leaves the other slots paused. Disconnecting or stopping cleans up only
owned virtual handles. Controller creation/identification remains serialized.

Unknown native connection status requires explicit reservations. Extra slots
wait unbound until assigned; the legacy `pdp-user` preference applies only to the
first virtual slot. Reservations do not override the console's own user picker.

Payload Manager: place the ELF and [metadata](../payload/LibrePadPS5.elf.json) together in `/data/pldmgr/payloads/LibrePadPS5/`. Older releases download under the previous ELF filename; rename the downloaded ELF to `LibrePadPS5.elf`. Keep one installed entry and stop the old instance before loading a replacement.

## Dashboard

**`http://<PS5_IP>:8096/`**: trusted LAN only, no authentication. Do not expose publicly.

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

These names describe the tested 8-byte preset. Other drivers normalize their own inputs to PS5 controls; use the dashboard to override outputs.

## Bluetooth

1. Confirm native DualSense input works and protected-user settings are configured. Keep a USB data cable ready. Do not run competing controller payloads.
2. Unplug USB gamepads and click **Release virtual pad**. Wait for removal.
3. Enter the target controller's Bluetooth address, available in its PC Bluetooth device details. Never enter the console's own DualSense address.
4. Acknowledge the shared-services warning and select **Enable & pair selected controller**. Put the controller in pairing mode. The window lasts 120 seconds.
5. When connected, click **Reconnect**, press **Home / PS** and assign the separate console user. Check native DualSense input immediately.

**Disable Bluetooth** closes our owned links and transport. **Forget selected controller** also removes its bond. Both are queued to the input thread; check status/log for the result. Bluetooth always starts disabled and must be enabled again after payload restarts. Bonds are stored in `bluetooth.db` with private file permissions, separately from other payloads.

Address approval covers Classic pairing, saved-bond reconnects and LE private-address identities. It cannot guarantee hardware isolation: the HCI event mask and pairing settings are shared. Unknown connection handles are never swept or disconnected. An unexpected chip or descriptor layout is refused rather than guessed.

If native controllers stop working, stop the payload and fully reboot. Do not update firmware to troubleshoot. Added Bluetooth support has only been tested in simulation, not on this console.

## Controller profiles & remapping

Requires the development build, not the older stable download. Release every virtual pad before saving shared mappings. Custom layouts are host-tested only; keep the previous release available.

1. Enter the controller’s four-digit hexadecimal VID/PID.
2. Choose a dedicated driver, Generic USB HID, Bluetooth button mapping, or `switch8` for the tested layout. For custom raw USB, enter report length, report ID, hat and axis byte offsets and click **Use custom layout**. Offsets include the report-ID byte. Use `-1` to disable a field.
3. Select each named PS5 input's output for driver profiles. Custom raw USB uses source bit numbers instead. **Default mapping** restores the tested 8-byte preset, without saving.
4. Click **Add / update in editor**, **Release all virtual pads**, then **Save & apply** after removal. Click **Reconnect**; newly added devices may need replugging.

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

Outputs: `none,cross,circle,square,triangle,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad,up,right,down,left`. Custom raw layouts support unsigned 8-bit axes and digital triggers. Dedicated drivers and descriptor-based generic HID support other axis formats and analog trigger pressure.

Driver formats: `driver:ds4`, `driver:ds3`, `driver:xinput`, `driver:switchpro`, `driver:8bitdo`, `driver:generic`, `driver:bluetooth`. Their 14 comma-separated outputs correspond to `triangle,circle,cross,square,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad`; no `@bit` suffixes. Known USB protocols work without a custom row. Saved rows override defaults for their exact VID/PID.

**View latest raw report** shows bytes from recognized/configured USB devices, even while paused. For initial investigation, capture on a PC using `python tools/usb_report_probe.py --vid 1234 --pid 5678` (requires `hidapi`). Do not guess layouts on the console. Sony USB access is restricted to the known DualShock 3/4 IDs; native DualSense IDs remain blocked. Explicit configuration is not proof that a third-party device cannot interfere with native input.

The original protected-user settings and native-device ownership checks still apply. Unknown USB IDs require a saved profile. Generic USB uses a real HID gamepad descriptor, not guessed report bytes. Generic Bluetooth reads the controller's descriptor. [Coverage and generic map files](CONTROLLERS.md).

## Troubleshooting / upgrades

- **Inactive pad:** click Reconnect, check the log and USB ID. If unassigned, press Home and select the separate user. For game focus, try Home/PS; game-specific behavior is not verified.
- **Skipped D-pad taps:** use the current release and stop older instances. Report recurring problems with logs.
- **DualSense cannot connect:** stop LibrePadPS5, unplug the USB controller and fully shut down the console, not Rest Mode. A development-session failure required a full reboot; its cause remains unconfirmed. If needed, [reset the DualSense](https://www.playstation.com/en-us/support/hardware/troubleshoot-dualsense/) and reconnect with a USB data cable. Do not update firmware just to troubleshoot.
- **Dashboard offline:** check the console IP, HTTP and port `8096`. It closes when the payload stops.
- **Stop through FTP:** upload an empty file named `stop` into the state directory.
- **Upgrade compatibility:** the on-console state directory is still `/data/pdp-pad/`; keep it and your profile files. The legacy `pdp-user` filename and lock remain compatible with previous builds. These paths are centralized in [legacy_paths.h](../src/core/legacy_paths.h); changing them without migration would lose profile protection and allow old/new instances to use separate locks. Historical releases keep their original assets.

The payload manages its own virtual pad. Bluetooth scanning is opt-in and address-filtered; no shell-process injection, native-handle fallback or autoload. It uses elevated credentials and shared controller services: crashes/service issues remain possible. Do not run competing controller payloads.

## Build

Docker with Linux containers, from the repo root:

```sh
docker build -t librepadps5-builder .
docker run --rm -v "$PWD:/work" -w /work librepadps5-builder
```

PowerShell: use `"${PWD}:/work"` for the volume. Output: `dist/LibrePadPS5.elf`. Builder uses **PS5 Payload SDK v0.43**. With the SDK installed: `make -f freepad.mk`. Dashboard assets are generated from `web/` during the build; no extra files need uploading.

Host tests (run inside a disposable builder container; they create the legacy state directory and do not contact a PS5):

Run all sanitizer-enabled suites with `make test`: USB protocols, configuration, HTTP, lifecycle, Classic/LE simulations, crypto, descriptors, fuzzing and adapter ownership/queue tests. This also generates dashboard assets. Use the disposable container, not the live console.

## Credits

Derived from [OmniPad PS5](https://github.com/diegobarbosaa/OmniPad-PS5) by diegobarbosaa. Bluetooth and generic HID use the pinned [AnyPad core](../vendor/anypad/README.md). Thanks to [PoorDS4/Ghostcontrol](https://github.com/a-ddr/PoorDS4), [YetAnotherControllerEnabler](https://github.com/MegaCadeDev/YetAnotherControllerEnabler) and [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) contributors. Original source attribution retained; Git history contains this project's commits only.

[GPL-3.0](../LICENSE). Not affiliated with Sony, Nintendo or controller manufacturers.
