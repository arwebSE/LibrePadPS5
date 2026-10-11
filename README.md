# LibrePadPS5

Use your gamepad on PS5.

[Download v0.5.0 preview](https://github.com/arwebSE/freepad-usb-ps5/releases/download/v0.5.0-preview/LibrePadPS5.elf) · [Stable releases](https://github.com/arwebSE/freepad-usb-ps5/releases) · [Issues](https://github.com/arwebSE/freepad-usb-ps5/issues)

## What it does

- USB drivers for DualShock 3/4, Xbox/XInput, Switch Pro and 8BitDo adapters.
- Descriptor-based generic USB HID and Bluetooth gamepads.
- Bluetooth Classic and LE, including Secure Connections pairing.
- Custom USB IDs, raw report layouts and named button remapping.
- LAN dashboard for input, profiles, pairing, PS, release/reconnect and logs.

**Experimental preview:** software tests pass; added controllers and Bluetooth are not physically verified with LibrePadPS5. The original wired controller `0e6f:0184` is verified on PS5 12.60. [Controller coverage and limits](docs/CONTROLLERS.md).

## Use

1. Create a separate PS5 user for the gamepad. Keep the DualSense on its own user.
2. Stop other controller payloads and configure [protected-user settings](docs/FREEPAD.md#setup).
3. Load `LibrePadPS5.elf`. Open `http://<PS5_IP>:8096/`.
   The **Controller** tab has connection controls; **Advanced** has profiles, remapping and diagnostics.
4. USB: plug in the controller, click **Reconnect**, then **Home / PS** and select its separate user.
5. Bluetooth: unplug USB gamepads, release the virtual pad, enter the controller address and select **Enable & pair** while the controller is in pairing mode. Bluetooth starts disabled.

Already using an older build? Keep `/data/pdp-pad/` and its profile files. Stop the old instance before loading this one.

## Remap

Choose the driver and VID/PID in the dashboard, change the named outputs, then **Add / update**, **Release**, **Save & apply**, **Reconnect**. Unknown USB devices can use **Generic USB HID** or a custom raw layout. [Configuration](docs/FREEPAD.md#controller-profiles--remapping-preview).

## Safety and limits

One gamepad at a time. LAN only, no authentication; do not expose the dashboard publicly. No game-driven rumble, controller audio, motion forwarding or touchpad movement.

Bluetooth uses shared console services. Address filtering and owned-handle cleanup reduce accidental interference but do not guarantee native DualSense isolation. If native input fails, stop the payload and fully reboot the console, not Rest Mode. Never select the console's own DualSense for pairing. No firmware updates are needed for this workflow.

[Setup, troubleshooting and build](docs/FREEPAD.md) · [Source map](src/README.md)

## Credits

Derived from [OmniPad PS5](https://github.com/diegobarbosaa/OmniPad-PS5), with the [AnyPad Bluetooth core](vendor/anypad/README.md). Thanks to PoorDS4/Ghostcontrol, YetAnotherControllerEnabler and PS5 Payload SDK contributors. [GPL-3.0](LICENSE). No upstream Git history is imported.
