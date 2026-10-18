# Controller compatibility

## Confirmed setups

| Controller | Connection | PS5 firmware | Verification |
|---|---|---|---|
| PDP Faceoff Deluxe+ (`0e6f:0184`) | Wired USB | 12.60 | Maintainer tested |
| Xbox One model 1697 + Microsoft Wireless Adapter (`045e:02e6`) | Xbox Wireless | 12.60 | Maintainer tested, including cold startup with bundled firmware |
| DualShock 3 (`054c:0268`) | Wired USB | 11.40 | [Tester confirmed](https://github.com/arwebSE/LibrePadPS5/issues/4#issuecomment-6228485447) |
| Xbox Series or DualShock 4 + 8BitDo USB Wireless Adapter 2 | USB adapter | 13.60 | [Tester confirmed](https://github.com/arwebSE/LibrePadPS5/issues/1#issuecomment-6115129726), including [games, emulators and homebrew](https://github.com/arwebSE/LibrePadPS5/issues/1#issuecomment-6115038058) |

PDP and Xbox Wireless also work together on separate PS5 profiles.

## Other driver coverage

These drivers are implemented; the table does not establish hardware verification.

| Controller family | USB | Bluetooth |
|---|---|---|
| DualShock 4 | Dedicated driver | Classic driver, including compatible Hori Onyx, Raiju and SCUF Vantage profiles |
| Xbox 360 / One / Series | XInput and Xbox One/Series drivers | Classic One S/Elite/Adaptive; LE Series/current firmware |
| Switch Pro / Nintendo NSO | Dedicated driver | Classic driver |
| Other 8BitDo and generic HID gamepads | Generic HID; configuration may be required | Built-in mappings or generic HID |

## Limits

- DualShock 3, Wii Remote, Wii U Pro and standalone Joy-Con are unsupported over Bluetooth.
- Bluetooth hardware remains unverified; external Bluetooth dongles are unsupported.
- Xbox Wireless handles one adapter and one controller at a time. Adding another adapter does not add another supported Xbox Wireless player. Microsoft adapter `045e:02fe` is implemented but unverified.
- Native DualSense/Edge USB controllers stay with the PS5 and are ignored by LibrePadPS5.
- Four-player game behavior remains unverified. Each virtual controller needs a separate PS5 user.
- Game-driven rumble, motion forwarding, controller audio and touchpad movement are not implemented.
- Xbox Wireless uses a fixed button mapping; dashboard remapping covers USB and built-in Bluetooth connections.

[Connection and remapping instructions](SETUP.md) · [Xbox Wireless setup](XBOX.md)
