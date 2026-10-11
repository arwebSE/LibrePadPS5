# FreePad USB

Wired Switch controller support for jailbroken PS5. No Bluetooth.

**[Download FreePad-USB.elf](https://github.com/arwebSE/freepad-usb-ps5/releases/latest/download/FreePad-USB.elf)** · [Releases](https://github.com/arwebSE/freepad-usb-ps5/releases) · [Issues](https://github.com/arwebSE/freepad-usb-ps5/issues)

[Controller profiles & remapping preview](https://github.com/arwebSE/freepad-usb-ps5/releases/tag/v0.4.0-preview): custom USB IDs for the existing `switch8` format, 14 button dropdowns, defaults, import/export and raw reports. Host/UI tested; not yet tested on PS5. [Usage](docs/FREEPAD.md#controller-profiles--remapping-preview).

## Compatibility

- **Hardware verified:** PDP Faceoff Deluxe+ Audio Wired Switch controller, USB ID **`0e6f:0184`**. Preview custom profiles only support the same `switch8` report format; changing IDs does not add a decoder for other layouts.
- **Tested:** PS5 **12.60**, buttons, sticks, D-pad and profile assignment. Individual games/emulators and other firmware are not explicitly verified.
- No rumble, motion, audio or touchpad movement. Digital triggers only.

## Setup

1. Create a **separate PS5 user** for the Switch controller. Keep your DualSense on its own user.
2. Stop other controller payloads. Follow the [first-time profile protection setup](docs/FREEPAD.md#setup) before enabling input.
3. Load **`FreePad-USB.elf`** and plug in the controller.
4. Open **`http://<PS5_IP>:8096/`**, click **Reconnect**, press **Home / PS**, then select the separate user.

Already using the previous build? **Keep your existing state directory and profile files.** Stop the old instance before loading this one. See [upgrade compatibility](docs/FREEPAD.md#troubleshooting--upgrades).

## Dashboard & buttons

The dashboard shows input and profiles, sends PS, releases/reconnects the virtual pad, displays logs and stops the payload. **LAN only; no authentication. Do not expose publicly.**

`B → Cross` · `A → Circle` · `Y → Square` · `X → Triangle`

`Home → PS` · `Capture → touchpad click` · `−/+ → Create/Options`

Sticks, D-pad, shoulders and stick clicks map normally.

## Problems?

Inactive pad: **Reconnect**, then check its profile and the log. DualSense connection failure: stop the payload, unplug the Switch controller and **fully reboot** the console—not Rest Mode. Do not update firmware just to troubleshoot. Native status marked *unavailable* does not mean disconnected.

[Full setup, Payload Manager, troubleshooting and build instructions](docs/FREEPAD.md)

Uses elevated credentials and shared controller services; crashes/service issues remain possible. No Bluetooth scanning, shell-process injection or autoload. Do not run competing controller payloads.

## Credits

Derived from [OmniPad PS5](https://github.com/diegobarbosaa/OmniPad-PS5). Thanks to AnyPad, PoorDS4/Ghostcontrol, YetAnotherControllerEnabler and PS5 Payload SDK contributors; [details](docs/FREEPAD.md#credits). [GPL-3.0](LICENSE). Not affiliated with Sony or Nintendo.
