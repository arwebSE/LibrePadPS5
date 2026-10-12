# LibrePadPS5

Use your favorite gamepad on a jailbroken PS5.

[Download v1.1.3](https://github.com/arwebSE/LibrePadPS5/releases/download/v1.1.3/LibrePadPS5.elf) · [Releases](https://github.com/arwebSE/LibrePadPS5/releases) · [Issues](https://github.com/arwebSE/LibrePadPS5/issues)

## Features

- Supports 1-4 gamepads.
- Wired USB, Xbox Wireless and Bluetooth connections.
- Support for PlayStation, Xbox, Switch and 8BitDo gamepads.
- A dashboard to connect controllers, change button mappings and download logs.

This project is still in early development. Bluetooth starts disabled. USB adapters such as 8BitDo handle their own pairing.

**Tested on PS5 12.60** with a PDP wired controller and an Xbox One controller using Microsoft's wireless adapter. Other setups, including Bluetooth and four-player games, still need testing. See [controller compatibility](docs/CONTROLLERS.md) for details.

**8BitDo USB Wireless Adapter 2** input is reported working on PS5 13.60 with Xbox Series and PS4 controllers, including games, emulators and homebrew. v1.1.3 includes the working preview's USB fixes and adds automatic controller recovery after logging out of every account. See [setup](docs/SETUP.md#8bitdo-usb-wireless-adapter-2).

## Get started

You'll need a jailbroken PS5 and an ELF loader or Payload Manager.

1. Stop any other controller payloads and leave your extra gamepads unplugged.
2. Download and load `LibrePadPS5.elf`, then open `http://<PS5_IP>:8096/` on your phone or computer. Replace `<PS5_IP>` with your console's IP address.
3. In **Advanced > Input & native controllers**, reserve the profiles used by your DualSense controllers.
4. Connect your gamepad. Select it under **Control gamepad**, click **Reconnect** if needed, and assign it to a separate signed-in PS5 profile.

Use a different profile for each player, with up to four players total including DualSense users. The game must support the number of players you want to use.

For wireless setup, follow the [Xbox Wireless guide](docs/XBOX.md#quick-install-on-windows), [Bluetooth guide](docs/SETUP.md#bluetooth) or [8BitDo Adapter 2 guide](docs/SETUP.md#8bitdo-usb-wireless-adapter-2). Xbox Wireless supports one controller; its setup helper installs the required firmware for you.

Upgrading? Stop the old version before loading the new one. v1.1.3 fixes the startup failure in v1.1.2 and keeps your saved settings.

## Help and customization

- [Setup and troubleshooting](docs/SETUP.md)
- [Controller compatibility and limitations](docs/CONTROLLERS.md)
- [Change button mappings](docs/SETUP.md#controller-profiles--remapping)
- [Build and development](docs/DEVELOPMENT.md)

Having trouble? In the dashboard, open **Advanced > Logs & support** to download a report. You can [attach it to a GitHub issue](docs/SETUP.md#share-a-diagnostic-report-on-github) if you'd like to help us troubleshoot. Review it before sharing; logs are optional.

## Credits

Shoutout to [thaikhoa](https://github.com/thaikhoa) for testing the 8BitDo USB Wireless Adapter 2 with Xbox Series and PS4 controllers, sharing logs and helping catch bugs.

Shoutout to [MODDED WARFARE](https://www.youtube.com/@MODDEDWARFARE) for covering similar tools and sharing setup instructions with the PlayStation homebrew community.

Built on [OmniPad PS5](https://github.com/diegobarbosaa/OmniPad-PS5), [AnyPad](vendor/anypad/README.md) and [xow](docs/XBOX.md#source-provenance). Thanks to PoorDS4/Ghostcontrol, YetAnotherControllerEnabler and PS5 Payload SDK contributors.

[GPL-3.0 license](LICENSE).
