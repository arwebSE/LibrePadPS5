# FreePad USB setup and reference

Use a supported wired Switch controller on a jailbroken PS5. USB only; no Bluetooth.

**[Download ELF](https://github.com/arwebSE/freepad-usb-ps5/releases/latest/download/FreePad-USB.elf)** · [Releases](https://github.com/arwebSE/freepad-usb-ps5/releases) · [Issues](https://github.com/arwebSE/freepad-usb-ps5/issues)

## Supported hardware

- **Hardware verified:** PDP Faceoff Deluxe+ Audio Wired Switch controller, USB ID `0e6f:0184`. Preview profiles can explicitly allow other IDs using the same `switch8` layout, but they are not physically verified.
- Tested on **PS5 firmware 12.60**: sticks, buttons, D-pad, dashboard and separate-profile assignment. Individual games/emulators and other firmware are not explicitly verified.
- No rumble, motion, controller audio or touchpad movement. Triggers are digital.

## Setup

Requires an existing jailbreak, ELF loader or Payload Manager and FTP access.

**Use a separate PS5 user for the Switch controller**, such as `SwitchCtl`. Do not assign it to your DualSense user.

1. Confirm your DualSense works. Stop AnyPad, OmniPad and older controller payloads.
2. Through FTP, create the state directory listed in [upgrade compatibility](#troubleshooting--upgrades) and upload an empty file named `pause` into it.
3. Load `FreePad-USB.elf`, plug in the controller and open **`http://<PS5_IP>:8096/`**.
4. Protect your DualSense user: open `/api/state` at that address, find its eight-digit user `id`, put only that ID in a plain-text file named `native-user`, and upload it into the state directory. Click **Stop payload**, then reload the ELF.
5. Click **Reconnect**, press **Home** or the dashboard's **Press Home / PS**, and select **`SwitchCtl`** in the console's user picker. Check both controllers.

Optional: save the Switch user's ID in `pdp-user` in the same directory for automatic assignment. Settings are read on startup; restart after editing. Never copy IDs from another console. Invalid or identical protected/preferred IDs prevent startup. Protection does not override the console's own user picker.

Payload Manager: place the ELF and [metadata](../payload/FreePad-USB.elf.json) together in `/data/pldmgr/payloads/FreePad-USB/`. Keep one installed entry and stop the old instance before loading a replacement.

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

Requires **v0.4.0-preview**, not the older stable download. One controller at a time. New behavior is host-tested only; keep the previous release available for recovery.

1. Open **Controller profiles** in the dashboard. Select an existing profile, or enter your controller's four-digit hexadecimal **VID/PID**.
2. Choose the outputs for **B/A/Y/X** or any of the other ten buttons. `none` disables a button; duplicate outputs are allowed. **Default mapping** restores the dropdowns only.
3. Click **Add / update in editor**. To remove a profile, delete its entire line from the editor.
4. Click **Release virtual pad** and wait for removal, then **Save & apply**. Validation/save failures leave existing profiles unchanged. Click **Reconnect** after saving; unplug/replug a newly added device if needed.

**Export file** downloads the editor contents. **Import file** loads a `.conf` into the editor without applying it; review it, then Save. **Reload saved** discards editor changes. Up to eight ID-specific profiles are stored atomically in `controllers.conf` in the state directory. Missing file uses the original controller/default mapping; invalid files refuse startup. Removing this new file while the payload is stopped restores defaults on the next load.

Only `switch8` is implemented: raw button bits **X,A,B,Y,L,R,ZL,ZR,Minus,Plus,L3,R3,Home,Capture**, followed by a hat at byte 2 (0–7; neutral 15), four unsigned axes at bytes 3–6 and padding at byte 7. No controller-specific initialization is sent. Other report formats need a decoder; do not add an ID merely to try arbitrary data. Sony vendor ID `054c` is rejected. Explicit configuration is not a guarantee that a third-party device cannot interfere with native input.

Profile line format (mapping order is the raw-bit order above, not the dashboard display order):

```text
0e6f:0184 switch8 triangle,circle,cross,square,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad
```

Targets: `none,cross,circle,square,triangle,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad`. Strict IDs, supported format, exactly 14 targets and unique ID pairs are required. D-pad/axes are not remapped. Digital trigger values follow the mapped L2/R2 outputs.

**View latest raw report** shows recent bytes from an explicitly configured, claimed device, even while its virtual pad is released. It does not capture unknown or native controllers. USB reads request eight bytes for this format; this is not automatic HID-layout detection. For other layouts, capture reports on a PC with `python tools/usb_report_probe.py --vid 1234 --pid 5678` (requires `hidapi`) and implement/test a separate decoder first.

The original protected-user settings and native-device ownership checks still apply. New IDs remain disallowed unless saved in a valid profile. There is no generic-parser fallback.

## Troubleshooting / upgrades

- **Inactive pad:** click Reconnect, check the log and USB ID. If unassigned, press Home and select the separate user. For game focus, try Home/PS; game-specific behavior is not verified.
- **Skipped D-pad taps:** use the current release and stop older instances. Report recurring problems with logs.
- **DualSense cannot connect:** stop FreePad, unplug the Switch controller and fully shut down the console—not Rest Mode. A development-session failure required a full reboot; its cause remains unconfirmed. If needed, [reset the DualSense](https://www.playstation.com/en-us/support/hardware/troubleshoot-dualsense/) and reconnect with a USB data cable. Do not update firmware just to troubleshoot.
- **Dashboard offline:** check the console IP, HTTP and port `8096`. It closes when the payload stops.
- **Stop through FTP:** upload an empty file named `stop` into the state directory.
- **Upgrade compatibility:** the on-console state directory is still `/data/pdp-pad/`; keep it and your profile files. The legacy `pdp-user` filename and lock remain compatible with previous builds. These paths are centralized in [freepad_compat.h](../src/freepad_compat.h); changing them without migration would lose profile protection and allow old/new instances to use separate locks. Historical releases keep their original assets.

The payload manages its own virtual pad; no Bluetooth scanning, shell-process injection, native-handle fallback or autoload. It still uses elevated credentials and shared controller services: crashes/service issues remain possible. Do not run competing controller payloads.

## Build

Docker with Linux containers, from the repo root:

```sh
docker build -t freepad-usb-builder .
docker run --rm -v "$PWD:/work" -w /work freepad-usb-builder
```

PowerShell: use `"${PWD}:/work"` for the volume. Output: `dist/FreePad-USB.elf`. Builder uses **PS5 Payload SDK v0.43**. With the SDK installed: `make -f freepad.mk`. Inherited `make ps5` builds upstream OmniPad, not FreePad.

Host tests (run inside a disposable builder container; they create the legacy state directory and do not contact a PS5):

Run all four suites with `make test`, or individually:

```sh
clang -std=c11 -Wall -Wextra -Werror -Isrc tests/test_freepad.c src/usb_controllers.c -o /tmp/parser-test && /tmp/parser-test
clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad_profiles.c src/freepad_profiles.c src/usb_controllers.c -o /tmp/profile-test && /tmp/profile-test
clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad_dashboard.c src/freepad_profiles.c src/usb_controllers.c -o /tmp/dashboard-test && /tmp/dashboard-test
clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad_lifecycle.c src/util.c -ldl -o /tmp/lifecycle-test && /tmp/lifecycle-test
```

## Credits

Derived from [OmniPad PS5](https://github.com/diegobarbosaa/OmniPad-PS5) by diegobarbosaa. Thanks to [AnyPad](https://github.com/sinfiltros/AnyPad-PS5), [PoorDS4/Ghostcontrol](https://github.com/a-ddr/PoorDS4), [YetAnotherControllerEnabler](https://github.com/MegaCadeDev/YetAnotherControllerEnabler) and [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) contributors. Original source attribution retained; Git history contains this project's commits only.

[GPL-3.0](../LICENSE). Not affiliated with Sony, Nintendo or controller manufacturers.
