# LibrePadPS5 setup

Requires a jailbroken PS5 and an ELF loader or Payload Manager.
[Check controller compatibility](CONTROLLERS.md) before starting.

[Download latest ELF](https://github.com/arwebSE/LibrePadPS5/releases/latest/download/LibrePadPS5.elf) · [Payload Manager JSON](https://github.com/arwebSE/LibrePadPS5/releases/latest/download/LibrePadPS5.elf.json)

Bundled Xbox firmware is currently in source builds; published downloads may
still request a manual install. See [source and release differences](README.md#source-builds-and-published-downloads).

## Setup

1. Confirm your DualSense works and stop other controller payloads, including any older LibrePadPS5 instance.
2. Load `LibrePadPS5.elf` and open **`http://<PS5_IP>:8096/`** on your phone or computer.
3. In **Advanced > Input & native controllers**, reserve the signed-in profiles used by your native DualSense controllers.
4. Connect your gamepad. For a Microsoft Xbox Wireless adapter, [pair through the adapter](XBOX.md#setup). Bundled-firmware builds handle its firmware automatically; older builds may request an install.
5. Select the gamepad card under **Your controllers** (the **Control gamepad** menu in older dashboards), click **Reconnect** if released, and assign a separate signed-in PS5 profile.
6. Press **Home / PS** if needed and choose that gamepad's profile. Check sticks and buttons in the dashboard, then try the controller on the PS5.

**Each gamepad needs its own PS5 user.** Keep your DualSense on its reserved profile.

LibrePadPS5 presents each supported gamepad to the PS5 as a **virtual pad**.
Reserving a PS5 profile keeps virtual pads from being assigned to that native
DualSense user. It does not change the DualSense's pairing or input.
The **Controller profiles** editor instead stores driver and button mappings;
those profiles are unrelated to PS5 user accounts.

For Payload Manager, place the ELF and matching JSON together in
`/data/pldmgr/payloads/LibrePadPS5/`. Keep one installed entry and stop it before
replacing the files. Existing settings and manually installed Xbox firmware
overrides are retained. Bundled firmware travels inside the ELF.

### DualShock 3 USB

Use a USB data cable and press the controller's physical PS button after
connecting. Reconnect/assign its virtual pad as above. DS3 USB does not need
Bluetooth pairing or Xbox firmware.

### 8BitDo USB Wireless Adapter 2

1. Plug in the adapter and pair your controller to it. Wait for its LED to stay **solid**.
2. Leave LibrePadPS5 Bluetooth disabled; the adapter handles the wireless connection.
3. Assign the gamepad to its own PS5 profile. Microsoft Xbox adapter firmware is not needed.

If paired but input does not work, try **XInput mode**:

- **Xbox Series / Xbox One:** hold **View + D-pad Up for three seconds** until the LED blinks twice. [Manual](https://support.8bitdo.com/Manual/USB-Adapter-2/xbox-windows.html).
- **DualShock 4:** hold **Share + D-pad Up for three seconds** until the LED blinks twice. [Manual](https://support.8bitdo.com/Manual/USB-Adapter-2/ps4-windows.html).

Unplug/replug the adapter after changing modes. USB ID `2dc8:3107` means the
adapter is idle and has not exposed a connected gamepad; check pairing first.

## Multiple controllers

Reserve each native DualSense user's profile, then connect and assign each
additional gamepad to a different signed-in user. For two DualSense controllers
plus PDP and Xbox, use four profiles total. The game must support that many players.

For example:

| Controller | PS5 user | LibrePadPS5 action |
|---|---|---|
| First DualSense | Player 1 | Reserve this user |
| Second DualSense | Player 2 | Reserve this user |
| PDP USB | Player 3 | Select the PDP and assign this user |
| Xbox Wireless | Player 4 | Select the Xbox and assign this user |

The app has four virtual slots, but native and virtual players share the PS5's
four signed-in users. In this example only two virtual pads are needed. Slot
numbers follow device connections; do not assume a particular slot is always
the Xbox or PDP. Select the device by name before using its controls.

Only one Xbox Wireless adapter/controller is handled at a time. PDP and Xbox
are confirmed working together; the full four-player game scenario remains
unverified. Native DualSense controllers stay managed by the PS5 and do not
appear as LibrePadPS5 gamepads. See [compatibility and limits](CONTROLLERS.md).

## Dashboard

Use the dashboard on a trusted LAN; it has no authentication. Do not expose it publicly.

The dashboard defaults to dark mode. Use the sun/moon button in the navbar to
switch themes; your browser remembers the choice.

- **Home / PS:** sends a PS-button press through the selected virtual pad.
- **Assign user:** binds the selected pad to an eligible signed-in profile.
- **Release virtual pad:** removes the selected virtual pad and keeps it paused across reloads/replugging.
- **Reconnect:** clears that pad's pause and creates it again.
- **Stop payload:** shuts down LibrePadPS5 cleanly.

Release/Reconnect affect the selected pad. **Release all virtual pads** pauses
all pads; reconnect each one you want to use. Native status **unavailable** does
not mean your DualSense is disconnected; reserve its profile explicitly.

## Bluetooth

Bluetooth is optional, starts disabled and has not been verified on hardware.
It uses shared console services; keep a USB cable available for your DualSense.

1. Unplug USB gamepads, including the Xbox Wireless Adapter, and release all virtual pads. Release alone does not disconnect their physical sources.
2. Enter the target gamepad's Bluetooth address, available in its PC Bluetooth device details. Never select your console's native DualSense.
3. Select **Enable & pair selected controller** and put it in pairing mode. The pairing window lasts 120 seconds.
4. Once connected, reconnect/assign its virtual pad and press **Home / PS**. Check your native DualSense still works.

**Disable Bluetooth** closes its connections. **Forget selected controller** also
removes its saved bond. Enable Bluetooth again after each payload restart.

## Controller profiles & remapping

Recognized controllers work without a custom profile.

The editor applies to USB and the optional built-in Bluetooth connection.
Xbox Wireless through Microsoft's adapter uses a fixed mapping; selecting
**Xbox / XInput USB** does not remap that wireless connection.

To change a supported mapping:

1. Open **Advanced > Controller profiles** and select the controller or enter its four-digit hexadecimal VID/PID.
2. Choose its driver/layout and map inputs to the desired PS5 buttons.
3. Click **Add / update in editor**, then **Release all virtual pads**.
4. After removal, click **Save & apply**, then reconnect each pad. New devices may need replugging.

**Default mapping** restores the tested PDP preset in the editor; it does not
save automatically. Use [the mapping reference](MAPPINGS.md) for custom raw
layouts, import/export formats, generic HID map files and the PDP button layout.

## Troubleshooting

- **Controller appears but does nothing:** reconnect it, assign its separate signed-in user and try physical Home/PS. Check whether dashboard input changes.
- **Assign user is unavailable:** wait for registration to finish, sign in the intended user and check that it is neither reserved for a native controller nor assigned to another virtual pad.
- **Xbox firmware setup is requested:** the ELF predates bundled firmware. Use the [manual install](XBOX.md#advanced), or build the current source. If an override is invalid, follow [Xbox troubleshooting](XBOX.md#troubleshooting).
- **Input stops after logging out:** wait for virtual-pad registration to finish, then press Home/PS. Reconnect manually if needed.
- **Dashboard offline / no startup notification:** check the console IP and port `8096`. If another instance is running, use its dashboard to stop it before reloading. Retrieve the log or loader output as described below.
- **DualSense stops working:** stop LibrePadPS5, unplug the third-party controller and fully shut down the PS5, not Rest Mode. Reconnect the DualSense by USB if needed. Do not update console firmware just to troubleshoot.
- **Storage startup failure:** use the latest release and keep your settings. See [storage recovery](STORAGE.md); do not delete or manually move folders.
- **Stop through FTP:** create an empty `/data/LibrePadPS5/stop` file.

## Share a diagnostic report on GitHub

1. Reproduce the problem, then open **Advanced > Logs & support > Download diagnostic report (.txt)** while the payload is running.
2. Review the file before sharing; it can contain profile names/IDs and device addresses.
3. Attach it to your existing issue, or use **Open GitHub issue** in the dashboard. Include the firmware, payload version, controller model, connection type, loader and what happened.

If the dashboard cannot open, retrieve `/data/LibrePadPS5/LibrePadPS5.log` through
FTP and attach it as `.txt`. If no new log exists, share the loader output and
any startup notification. The FTP log also contains history beyond the
dashboard's download limit.

[Documentation index](README.md) · [Storage and migration](STORAGE.md) · [Build and development](DEVELOPMENT.md) · [Credits](../README.md#credits)
