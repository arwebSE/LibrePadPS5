# Controller coverage

Code coverage is not hardware verification. All restored drivers and Bluetooth are experimental in LibrePadPS5. The PDP `0e6f:0184` wired gamepad and Xbox One model 1697 through `045e:02e6` are verified on PS5 12.60, including simultaneous active pads assigned to different profiles. Targeted PDP release/reconnect leaves Xbox injection running. Other models and game-specific four-player multiplayer remain unverified.

| Family | USB | Bluetooth |
|---|---|---|
| DualShock 4 | Dedicated driver; Sony IDs `05c4`, `09cc`, `0ba0` | Dedicated Classic driver; DS4, Hori Onyx, Raiju, SCUF Vantage profiles |
| DualShock 3 | Dedicated driver, `054c:0268` | Not implemented upstream or here |
| Xbox | 360/XInput and One/Series GIP input, wake-up and Guide ACK | Classic One S/Elite/Adaptive; LE Series/current firmware; Secure Connections |
| Switch Pro / NSO | Full-report driver and USB wake-up | Classic driver and report-mode initialization |
| 8BitDo | Known adapters use XInput; other HID modes can use Generic USB HID | Descriptor-based generic HID, including upstream mappings |
| GameSir, iPega, SteelSeries, MOGA, Shield, Atari, Hyperkin, Mocute and other HID pads | Generic USB HID after adding an explicit profile if not auto-recognized | All AnyPad built-in generic mappings retained; other HID pads use descriptor/default mapping |
| DualSense / Edge | Never claimed by our USB scanner | Upstream decoder retained, selected address only. Do not select the console's native controller |
| Wii Remote, Wii U Pro, standalone Joy-Con | No dedicated driver | Not implemented upstream or here |

The exact Bluetooth ID tables and mappings are in [profiles.c](../vendor/anypad/profiles.c) and [generic.c](../vendor/anypad/generic.c), pinned to the [documented AnyPad commit](../vendor/anypad/README.md). Firmware-dependent Bluetooth layouts remain unverified on actual hardware.

## Xbox Wireless and simultaneous pads

The optional xow-based radio module supports the Microsoft adapter `045e:02e6`
(hardware verified) and allowlists `045e:02fe` (unverified). It handles one Xbox
controller alongside USB controllers through the shared three-slot manager.
Microsoft firmware is supplied separately and is not distributed in the payload.
See [radio setup and limits](../experimental/xbox/README.md).

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

For unusual generic HID axis/button usages, place `VVVV_PPPP.map` inside `/data/pdp-pad/maps/` using hexadecimal IDs. [Example](../maps/EXAMPLE.map). Disable/re-enable Bluetooth or replug USB after editing a map. Generic reports retain state across separate report IDs.

USB wake-up/Guide protocol details were checked against Linux's [xpad](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) and [hid-nintendo](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-nintendo.c) drivers.
