# LibrePadPS5 setup and reference

Use a wired USB, Xbox Wireless or Bluetooth gamepad on a jailbroken PS5. See [tested hardware and controller coverage](CONTROLLERS.md).

**[Download latest ELF](https://github.com/arwebSE/LibrePadPS5/releases/latest/download/LibrePadPS5.elf)** · [Releases](https://github.com/arwebSE/LibrePadPS5/releases) · [Issues](https://github.com/arwebSE/LibrePadPS5/issues)

## Supported hardware

- **Hardware verified:** PDP Faceoff Deluxe+ Audio Wired Switch controller, USB ID `0e6f:0184`. The current release also accepts configurable HID layouts; Xbox One model 1697 with Xbox Wireless Adapter `045e:02e6` is also verified individually on firmware 12.60; the adapter needs separately staged firmware.
- Tested on **PS5 firmware 12.60**: sticks, buttons, D-pad, dashboard and separate-profile assignment. Individual games/emulators and other firmware are not explicitly verified.
- **Tester reported:** 8BitDo USB Wireless Adapter 2 works on PS5 13.60 with Xbox Series and PS4 controllers, including PS4/PS5 games, emulators and homebrew. [Controller report](https://github.com/arwebSE/LibrePadPS5/issues/1#issuecomment-6115129726), [game coverage](https://github.com/arwebSE/LibrePadPS5/issues/1#issuecomment-6115038058). Exact game titles remain unconfirmed.
- No game-driven rumble, motion forwarding, controller audio or touchpad movement. Dedicated drivers preserve analog triggers where available; the 8-byte preset has digital triggers.

## Setup

Requires an existing jailbreak and ELF loader or Payload Manager. Normal setup uses the dashboard; FTP is available for troubleshooting and manual legacy configuration.

Settings are stored in `/data/LibrePadPS5`, with automatic migration
from older locations.
See [storage paths and migration](STORAGE.md) before doing manual FTP configuration.

**Use a separate PS5 user for the gamepad**, such as `Gamepad`. Do not assign it to your DualSense user.

1. Confirm your DualSense works. Stop AnyPad, OmniPad and older controller payloads.
2. Leave third-party gamepads and the Xbox receiver unplugged. Load `LibrePadPS5.elf` and open **`http://<PS5_IP>:8096/`**.
3. In **Advanced / Input & native controllers**, reserve each signed-in profile used by a native DualSense. The dashboard saves the reservations immediately; no ID files or restart are needed. Existing `native-user` protection remains in force.
4. Plug in your gamepad. For Xbox Wireless, use [the setup helper or dashboard firmware upload](XBOX.md#firmware), then unplug/replug the adapter and pair.
5. Select the gamepad, click **Reconnect** if released, and assign a separate signed-in profile. If needed, press **Home / PS** and choose **`Gamepad`** in the console's user picker. Check both controllers. Active virtual controllers automatically renew after every account logs out; wait for registration to finish, then press Home.

Optional: save the gamepad user's ID in `controller-user` (`pdp-user` on older releases) in the same directory for automatic assignment. Settings are read on startup; restart after editing. Never copy IDs from another console. Invalid or identical protected/preferred IDs prevent startup. Protection does not override the console's own user picker.

### DualShock 3 USB

Use the latest release with a USB data cable. Connect the DS3 and press its physical
PS button. Select it in the dashboard, click **Reconnect** if released, and assign
a separate signed-in profile. Its sticks and buttons should update the input
display. Reserve the profile used by your native DualSense first.

[h1z1z1h16584 tested the fix on firmware 11.40](https://github.com/arwebSE/LibrePadPS5/issues/4#issuecomment-6228485447)
using the PS5 controller overlay. USB support does not require a Bluetooth adapter
or Xbox firmware. This confirmation applies to DS3 USB, not DS3 Bluetooth.

### Multiple controllers

USB and Xbox Wireless share four virtual slots. Native DualSense controllers
remain native and do not consume these slots. For two DualSense controllers plus
PDP and Xbox, use four separate signed-in profiles:

1. Reserve both native profiles in **Advanced / Input & native controllers**.
   The original `native-user` reservation remains protected. Additional IDs are
   saved atomically, one hexadecimal ID per line, in `native-users`.
2. Connect the PDP and Xbox receiver. Install the Xbox firmware as described in
   [the Xbox setup guide](XBOX.md#firmware).
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
wait unbound until assigned; the `controller-user` preference (`pdp-user` on older releases) applies only to the
first virtual slot. Reservations do not override the console's own user picker.

[PS5 supports four simultaneous controllers](https://www.playstation.com/en-ph/support/hardware/dualsense-controller-support/)
and [four signed-in users](https://blog.playstation.com/2020/11/09/ps5-the-ultimate-faq/).
The virtual-slot capacity does not promise extra players beyond those four users.
Native reservations reduce the available profiles; unused slots cost no extra virtual devices.

Payload Manager: place the ELF and [metadata](../config/payload/LibrePadPS5.elf.json) together in `/data/pldmgr/payloads/LibrePadPS5/`. Older releases download under the previous ELF filename; rename the downloaded ELF to `LibrePadPS5.elf`. Keep one installed entry and stop the old instance before loading a replacement.

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

Release every virtual pad before saving shared mappings. Custom layouts are host-tested only; keep the previous release available.

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

## 8BitDo USB Wireless Adapter 2

Use the [latest ELF](https://github.com/arwebSE/LibrePadPS5/releases/latest/download/LibrePadPS5.elf)
for this adapter. USB input is reported working on
PS5 13.60 with Xbox Series and PS4 controllers. This is a USB connection to LibrePadPS5; the adapter
handles the controller's wireless connection.

Plug in the adapter, pair your controller to it and wait for its LED to stay
**solid**. A tester confirms [plug, pair and play without changing modes](https://github.com/arwebSE/LibrePadPS5/issues/1#issuecomment-6115150398)
with Xbox Series and PS4 controllers. Leave LibrePadPS5 Bluetooth disabled: the
adapter handles its own wireless pairing and presents a USB gamepad.

If pairing succeeds but input does not work, try selecting **XInput mode**:

- **Xbox Series / Xbox One:** hold **View + D-pad Up for three seconds** until the LED blinks twice. [Xbox manual](https://support.8bitdo.com/Manual/USB-Adapter-2/xbox-windows.html).
- **DualShock 4:** hold **Share + D-pad Up for three seconds** until the LED blinks twice. [PS4 manual](https://support.8bitdo.com/Manual/USB-Adapter-2/ps4-windows.html).

After changing modes, unplug and replug the adapter, then assign a separate PS5
profile. You do not need Microsoft's Xbox adapter firmware for an 8BitDo adapter.

The tester also reports working input in PS4/PS5 games, emulators and homebrew.
After logging every account out, Home/PS initially does nothing, including the
dashboard's Home button. They [clarified that reconnecting the controller makes
it register again and allows selection of their main account from the login
screen](https://github.com/arwebSE/LibrePadPS5/issues/1#issuecomment-6115078167).
Guest also works. Active controllers automatically re-register after every account logs
out, preserving the physical USB connection. Wait for the controller to become
active, then press Home and choose a profile. This new recovery path is covered
by host tests and still needs console confirmation. If it fails, reconnect the
controller manually and optionally share a diagnostic report around logout.

The tester reports both DInput and XInput selections working with Xbox Series
and PS4 controllers. Their logs show the same `2dc8:3106` XInput interface for
both selections. USB connection notifications and logs display the detected
protocol and ID, rather than guessing the selected mode or paired controller.
Manual XInput selection is troubleshooting advice. Internal Bluetooth stays disabled
unless explicitly enabled; USB use does not enable it.

XInput receivers may identify as Xbox 360 `045e:028e` or an 8BitDo USB ID.
The payload recognizes
`2dc8:3106` and `2dc8:3109` alongside `310a`/`310b`. Other HID modes still require
a valid gamepad report descriptor. Only the reported Adapter 2 setup has
hardware confirmation; other modes and models remain unverified. If input fails, include the dashboard log, VID/PID,
selected adapter mode and paired controller model in the issue.

If the log shows **`2dc8:3107`**, the adapter is exposing its **idle receiver**
identity rather than a connected gamepad. The payload logs a waiting message
once and keeps watching for a gamepad identity; it does not claim the idle
interface or repeatedly reject its descriptor. Seeing the PS5 profile picker
does not confirm USB input. Check **View latest raw report** while pressing a
button after pairing and selecting XInput. If it still shows no reports, attach
a diagnostic download from that attempt, including the lines immediately after
replugging. Please include whether the adapter LED stays solid. The current log
also records the selected driver/interface/endpoints and distinguishes report
descriptor read failures from descriptors that contain no gamepad controls.

An Adapter 2 tester's earlier run reached **Ready** on PS5 13.60 but received
no input reports. The USB transport fixes in the current release resolved that
reported failure. [Investigation and tester feedback](https://github.com/arwebSE/LibrePadPS5/issues/1).

## Share a diagnostic report on GitHub

Logs are optional. A GitHub account is needed to post an issue or comment.

1. Stop any old instance, load the current release and reproduce the problem.
2. Open `http://<PS5_IP>:8096/` from a computer or phone while the payload is still running.
3. Select **Advanced > Logs & support > Download diagnostic report (.txt)**. This downloads a file locally.
4. Open and review the file. Logs may include profile names/IDs and Bluetooth/device addresses; remove anything you do not want to share publicly.
5. For an existing report, drag the `.txt` into its GitHub comment box or use GitHub's attachment control. For a new problem, use **Open GitHub issue** in the dashboard and fill in the [controller/startup issue form](https://github.com/arwebSE/LibrePadPS5/issues/new?template=controller_problem.yml). Attach the `.txt` in the optional diagnostic-report field, then submit.
6. Include your firmware, payload version, controller/adapter model, connection/adapter mode, ELF loader name/version and what you did before the failure.

The report includes the current version/firmware, per-pad state, Bluetooth status,
latest raw USB report, saved controller mappings and up to **256 KiB** of the
most recent payload log. It marks truncated or missing logs explicitly. Bond
keys and account-list files are not exported. The download and GitHub form link
require you to attach and submit the report yourself.

If the dashboard never opens, retrieve `/data/LibrePadPS5/LibrePadPS5.log` through FTP
and attach that `.log` file renamed to `.txt`, or paste the relevant lines. If
there is no new log, include the ELF loader's output and which notification you
saw (including no notification). For a complete log older than the dashboard
limit, retrieve the same file through FTP. For older releases, see [storage paths](STORAGE.md).

## Troubleshooting / upgrades

- **Inactive pad:** click Reconnect, check the log and USB ID. If unassigned, press Home and select the separate user, or assign a signed-in profile in the dashboard. Active controllers automatically renew their registrations after every account logs out; wait for registration to finish. If recovery fails, reconnect the controller manually. For game focus, try Home/PS.
- **Skipped D-pad taps:** use the current release and stop older instances. Report recurring problems with logs.
- **DualSense cannot connect:** stop LibrePadPS5, unplug the USB controller and fully shut down the console, not Rest Mode. A development-session failure required a full reboot; its cause remains unconfirmed. If needed, [reset the DualSense](https://www.playstation.com/en-us/support/hardware/troubleshoot-dualsense/) and reconnect with a USB data cable. Do not update firmware just to troubleshoot.
- **No startup notification / firmware 13.60:** the payload announces `starting` before opening its state/lock files and `ready` after its dashboard opens. Startup failures report the state-directory/lock, controller configuration, firmware check or virtual-pad initialization stage. If it says already running, open the existing dashboard and stop that instance before reloading. If no message appears, check `http://<PS5_IP>:8096/` and retrieve `/data/LibrePadPS5/LibrePadPS5.log` through FTP; confirm the new startup line matches the version you downloaded. For older releases, see [storage paths](STORAGE.md). No new log entry can mean the loader/SDK failed before reaching the entry point; include loader name/version and its output. Adapter 2 input on 13.60 is tester confirmed, but the original no-notification report still awaits feedback. Do not repeatedly inject while a previous instance is active.
- **Cannot prepare storage / only `instance.lock` appears:** load the latest release without deleting settings; it can recover an interrupted storage migration. If it still fails, share the complete error notification or ELF loader output, including the operation, path and errno. The log may not exist because initialization stopped before logging. [Storage recovery](STORAGE.md).
- **Dashboard offline:** check the console IP, HTTP and port `8096`. It closes when the payload stops.
- **Stop through FTP:** upload an empty file named `stop` into the state directory.
- **Upgrade compatibility:** settings automatically migrate to `/data/LibrePadPS5`. Keep your existing settings when upgrading; see [storage paths and migration](STORAGE.md). Older release paths are documented there.

The payload manages its own virtual pad. Bluetooth scanning is opt-in and address-filtered; no shell-process injection, native-handle fallback or autoload. It uses elevated credentials and shared controller services: crashes/service issues remain possible. Do not run competing controller payloads.

## Build

Docker with Linux containers, from the repo root:

```sh
docker build -t librepadps5-builder config/build
docker run --rm -v "$PWD:/work" -w /work librepadps5-builder
```

PowerShell: use `"${PWD}:/work"` for the volume. Output: `dist/LibrePadPS5.elf`. Builder uses **PS5 Payload SDK v0.43**. With the SDK installed, run `make`. Use `make WITH_XBOX=0` for USB/Bluetooth only, `make xbox` for the standalone Xbox diagnostic payload, and `make help` to list commands. Dashboard assets are generated from `web/` during the build; no extra files need uploading.

Host tests (run inside a disposable builder container; they create the legacy state directory and do not contact a PS5):

Run all 21 sanitizer-enabled C/C++ suites with `make test`: USB protocols and transport, configuration, HTTP, lifecycle, Classic/LE simulations, crypto, descriptors, fuzzing, adapter ownership/queues and Xbox transport/radio tests. This also generates dashboard assets. Run `make dashboard-test` separately with Node.js installed for tab, keyboard and GitHub reporting checks. Use the disposable container, not the live console. [Repository layout and contribution checks](DEVELOPMENT.md).

## Credits

Thanks to [thaikhoa](https://github.com/thaikhoa) for 8BitDo USB Wireless Adapter 2 testing and [h1z1z1h16584](https://github.com/h1z1z1h16584) for DualShock 3 USB testing.

Derived from [OmniPad PS5](https://github.com/diegobarbosaa/OmniPad-PS5) by diegobarbosaa. Bluetooth and generic HID use the pinned [AnyPad core](../vendor/anypad/README.md). Thanks to [PoorDS4/Ghostcontrol](https://github.com/a-ddr/PoorDS4), [YetAnotherControllerEnabler](https://github.com/MegaCadeDev/YetAnotherControllerEnabler) and [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) contributors. Original source attribution retained; Git history contains this project's commits only.

[GPL-3.0](../LICENSE). Not affiliated with Sony, Nintendo or controller manufacturers.
