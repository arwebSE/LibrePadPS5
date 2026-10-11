# Controller coverage

The PDP `0e6f:0184` wired gamepad and Xbox One model 1697 through `045e:02e6` are verified on PS5 12.60, including simultaneous active pads assigned to different profiles. Targeted PDP release/reconnect leaves Xbox injection running. A tester [reports the 8BitDo USB Wireless Adapter 2 working on PS5 13.60 with v1.1.1-preview](https://github.com/arwebSE/LibrePadPS5/issues/1#issuecomment-6114971637); their follow-up log shows 20-byte input reports and button presses. They [confirm Xbox Series and PS4 controllers](https://github.com/arwebSE/LibrePadPS5/issues/1#issuecomment-6115129726) and [PS4/PS5 game, emulator and homebrew input](https://github.com/arwebSE/LibrePadPS5/issues/1#issuecomment-6115038058). v1.1.1 includes those USB fixes. Exact game titles remain unconfirmed. Other models, direct Bluetooth hardware and game-specific four-player multiplayer remain unverified. The tables below describe implemented drivers; software coverage is distinct from hardware verification.

**Logout recovery:** the preview required reconnecting the controller after every account logged out; the tester [confirmed that this allowed selection of the main account from the login screen](https://github.com/arwebSE/LibrePadPS5/issues/1#issuecomment-6115078167). v1.1.1 automatically renews active virtual controller registrations after the last user logs out. This new recovery is host-tested and still needs console confirmation. Released pads remain released, and physical USB/radio connections remain intact.

The tester reports both DInput and XInput selections working and [plug, pair and play without changing modes](https://github.com/arwebSE/LibrePadPS5/issues/1#issuecomment-6115150398). Both logs expose the same `2dc8:3106` XInput interface. Notifications name the detected USB protocol; they cannot identify a requested adapter mode or the controller paired behind an identical USB interface.

| Family | USB | Bluetooth |
|---|---|---|
| DualShock 4 | Dedicated driver; Sony IDs `05c4`, `09cc`, `0ba0` | Dedicated Classic driver; DS4, Hori Onyx, Raiju, SCUF Vantage profiles |
| DualShock 3 | Dedicated driver, `054c:0268` | Unsupported by the bundled AnyPad core |
| Xbox | 360/XInput and One/Series GIP input, wake-up and Guide ACK | Classic One S/Elite/Adaptive; LE Series/current firmware; Secure Connections |
| Switch Pro / NSO | Full-report driver and USB wake-up | Classic driver and report-mode initialization |
| 8BitDo | Adapter 2 USB fixes included in v1.1.1; Xbox Series and PS4 controllers reported working on PS5 13.60. [Setup](SETUP.md#8bitdo-usb-wireless-adapter-2); other HID modes can use Generic USB HID | Descriptor-based generic HID, including upstream mappings |
| GameSir, iPega, SteelSeries, MOGA, Shield, Atari, Hyperkin, Mocute and other HID pads | Generic USB HID after adding an explicit profile if not auto-recognized | All AnyPad built-in generic mappings retained; other HID pads use descriptor/default mapping |
| DualSense / Edge | Never claimed by our USB scanner | Upstream decoder retained, selected address only. Do not select the console's native controller |
| Wii Remote, Wii U Pro, standalone Joy-Con | No dedicated driver | Unsupported by the bundled AnyPad core |

The exact Bluetooth ID tables and mappings are in [profiles.c](../vendor/anypad/profiles.c) and [generic.c](../vendor/anypad/generic.c), pinned to the [documented AnyPad commit](../vendor/anypad/README.md). Firmware-dependent Bluetooth layouts remain unverified on actual hardware.

AnyPad's [compatibility list at that commit](https://github.com/Mohammad-Aghayan/AnyPad-PS5-English/blob/9475e9f88b7d06f890ca3bb3f16537bcc503eff4/COMPATIBILITY.md) explicitly lists DualShock 3, Wii Remote, Wii U Pro and standalone Joy-Con as unsupported over Bluetooth. Generic HID support does not establish support for those controllers. A converter presenting one as a supported USB gamepad is a separate setup; compatibility depends on the converter's output mode.

## Xbox Wireless and simultaneous pads

The optional xow-based radio module supports the Microsoft adapter `045e:02e6`
(hardware verified) and allowlists `045e:02fe` (unverified). It handles one Xbox
controller alongside USB controllers through the shared four-slot manager in v1.1.0.
Microsoft firmware is supplied separately and is not distributed in the payload.
See [radio setup and limits](XBOX.md).

Native DualSense USB devices are ignored. Reserve each native user's profile,
then assign PDP and Xbox to separate other profiles. USB and Xbox do not enable
the shared Bluetooth host; Bluetooth remains an exclusive opt-in mode.

## USB detection

Exact OmniPad protocol IDs are recognized automatically. Its broad vendor-only guesses are replaced by HID descriptor validation for generic devices. Keyboards, mice and non-gamepad application collections are not accepted by generic USB setup. New IDs can select a dedicated driver or Generic USB HID in the dashboard; a VID/PID alone does not prove protocol compatibility.

The USB transport supports one HID gamepad interface, or an XInput/GIP vendor interface. Composite devices with multiple non-boot HID interfaces may need a dedicated transport adjustment. Unsupported report sizes, proprietary modes, third-party initialization sequences or missing endpoints can still prevent a device from working.

## Bluetooth detection

Classic L2CAP/SDP/HID and LE GATT/HID/SMP are included, with bond storage and reconnects. Pairing only accepts the entered address; private-address LE reconnects use the bonded identity. Nothing is automatically enabled at payload startup.

Internal HCI access is restricted to MediaTek `0e8d:3603`, two complete HCI functions and the known first-function layout. Other layouts fail closed. The second HCI function is never selected. This check is not proof of isolation across PS5 hardware/firmware. External Bluetooth dongles are not implemented.

## Remapping

Driver profiles remap canonical PS5 inputs in this order:

`triangle,circle,cross,square,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad`

The mapping preserves stick and D-pad state. Analog trigger pressure follows trigger-output remapping. Use `driver:bluetooth` for Bluetooth button overrides; it does not enable that ID for USB scanning.

For unusual generic HID axis/button usages, place `VVVV_PPPP.map` inside `/data/LibrePadPS5/maps/` using hexadecimal IDs. [Example](../config/maps/EXAMPLE.map). [Older releases and migration](STORAGE.md). Disable/re-enable Bluetooth or replug USB after editing a map. Generic reports retain state across separate report IDs.

USB wake-up/Guide protocol details were checked against Linux's [xpad](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) and [hid-nintendo](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-nintendo.c) drivers.
