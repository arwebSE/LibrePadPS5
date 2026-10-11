# LibrePadPS5

Use your gamepad on PS5.

[Download v0.5.3](https://github.com/arwebSE/LibrePadPS5/releases/download/v0.5.3-preview/LibrePadPS5.elf) · [Releases](https://github.com/arwebSE/LibrePadPS5/releases) · [Issues](https://github.com/arwebSE/LibrePadPS5/issues)

## What it does

- Up to three independent virtual gamepads, including wired USB plus one Xbox Wireless controller.
- USB drivers for DualShock 3/4, Xbox/XInput, Switch Pro and 8BitDo adapters.
- Descriptor-based generic USB HID and Bluetooth gamepads.
- Bluetooth Classic and LE, including Secure Connections pairing.
- Custom USB IDs, raw report layouts and named button remapping.
- LAN dashboard with per-gamepad controls, native-profile reservations, input, remapping and logs.

**Tested on PS5 12.60:** the PDP wired controller `0e6f:0184` and Xbox One model 1697 through adapter `045e:02e6` run simultaneously with separate profiles. Software regression checks pass. Other controller models, Bluetooth hardware and game-specific four-player behavior remain unverified. [Controller coverage and limits](docs/CONTROLLERS.md).

## Use

1. Use a separate PS5 profile for every player. Reserve all native DualSense profiles in Advanced.
2. Stop other controller payloads and configure [protected-user settings](docs/FREEPAD.md#setup).
3. Load `LibrePadPS5.elf`. Open `http://<PS5_IP>:8096/`.
   The **Controller** tab has connection controls; **Advanced** has profiles, remapping and diagnostics.
4. USB/Xbox: select each gamepad in **Control gamepad**, click **Reconnect** if released, then assign its separate signed-in profile. [8BitDo Adapter 2 requires manual XInput mode](docs/FREEPAD.md#8bitdo-usb-wireless-adapter-2). Xbox Wireless requires [separately staged firmware](modules/xbox/README.md#firmware).
5. Bluetooth: unplug USB gamepads, release the virtual pad, enter the controller address and select **Enable & pair** while the controller is in pairing mode. Bluetooth starts disabled.

Already using an older build? Keep `/data/pdp-pad/` and its profile files. Stop the old instance before loading this one.

Having trouble? [Download a diagnostic report and attach it to a GitHub issue](docs/FREEPAD.md#share-a-diagnostic-report-on-github). Logs are optional.

## Remap

Choose the driver and VID/PID in the dashboard, change the named outputs, then **Add / update**, **Release all virtual pads**, **Save & apply**, then reconnect each pad. Unknown USB devices can use **Generic USB HID** or a custom raw layout. [Configuration](docs/FREEPAD.md#controller-profiles--remapping).

## Safety and limits

Up to three virtual slots, with one Xbox Wireless controller. Native DualSense USB devices are excluded from capture. Reserve both native profiles for a two-DualSense setup; never select those profiles in the console picker. Local multiplayer also depends on the game supporting that player count. LAN only, no authentication; do not expose the dashboard publicly. No game-driven rumble, controller audio, motion forwarding or touchpad movement.

Bluetooth uses shared console services. Address filtering and owned-handle cleanup reduce accidental interference but do not guarantee native DualSense isolation. If native input fails, stop the payload and fully reboot the console, not Rest Mode. Never select the console's own DualSense for pairing. No firmware updates are needed for this workflow.

[Setup, troubleshooting and build](docs/FREEPAD.md) · [Source map](src/README.md)

## Credits

Derived from [OmniPad PS5](https://github.com/diegobarbosaa/OmniPad-PS5), with the [AnyPad Bluetooth core](vendor/anypad/README.md) and optional [xow radio port](modules/xbox/README.md#source-provenance). Thanks to PoorDS4/Ghostcontrol, YetAnotherControllerEnabler and PS5 Payload SDK contributors. [GPL-3.0](LICENSE). No upstream Git history is imported.
