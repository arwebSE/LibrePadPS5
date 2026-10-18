# Controller mapping reference

For dashboard steps, see [controller profiles and remapping](SETUP.md#controller-profiles--remapping).
Known controllers do not need a custom profile.

## Saved profiles

Import/export uses the editor contents. Up to eight profiles are saved in
`/data/LibrePadPS5/controllers.conf`. Each row applies to its exact hexadecimal
VID/PID and overrides that device's defaults. Invalid settings are rejected;
an invalid saved file prevents startup.

### Driver mappings

Available formats: `driver:ds4`, `driver:ds3`, `driver:xinput`, `driver:switchpro`,
`driver:8bitdo`, `driver:generic`, `driver:bluetooth`.

Each row has 14 comma-separated output names for these source inputs, in order:

`triangle,circle,cross,square,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad`

Use named outputs without `@bit` suffixes. Stick and D-pad state is preserved;
analog trigger pressure follows trigger-output remapping. `driver:bluetooth`
changes Bluetooth button mappings and does not enable USB detection for that ID.

### Tested PDP preset

`switch8` uses the tested 8-byte report: button bits X,A,B,Y,L,R,ZL,ZR,Minus,Plus,
L3,R3,Home,Capture; hat byte 2 (0-7, neutral 15); unsigned axes in bytes 3-6.

```text
0e6f:0184 switch8 triangle,circle,cross,square,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad
```

Its default mapping uses physical positions:

- B -> Cross, A -> Circle, Y -> Square, X -> Triangle.
- L/R -> L1/R1, ZL/ZR -> L2/R2, Minus/Plus -> Create/Options.
- Home -> PS, Capture -> touchpad click.

### Custom raw USB layouts

Format: `hid:length:reportID:hatByte:neutralHat:lxByte:lyByte:rxByte:ryByte`.

- Report length: 1-64 bytes. Byte offsets include the report ID.
- Use `-1` for an absent report ID, hat or axis.
- The hat uses its byte's low nibble; neutral must be 8-15.
- Axes are unsigned 8-bit; buttons are active-high and triggers are digital.
- The 14 output bindings use `output@absoluteBit`, with least-significant bit first.

Example only: 12 bytes, report ID 7, no hat, axes at bytes 4-7, Cross at byte 8
bit 0, followed by the D-pad. This is not a verified controller layout.

```text
1234:5678 hid:12:7:-1:15:4:5:6:7 cross@64,up@65,right@66,down@67,left@68,none@69,none@70,none@71,none@72,none@73,none@74,none@75,none@76,none@77
```

Output names: `none,cross,circle,square,triangle,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad,up,right,down,left`.

Use **View latest raw report** or capture on a PC with
`python tools/usb_report_probe.py --vid 1234 --pid 5678` (requires `hidapi`)
before defining a layout. Native DualSense USB devices remain blocked.

## Generic HID map files

For unusual generic HID usages, place `VVVV_PPPP.map` in
`/data/LibrePadPS5/maps/`, using hexadecimal VID/PID. See the
[example map](../config/maps/EXAMPLE.map). Replug USB or disable/re-enable
Bluetooth after editing. These files apply only to generic HID drivers.
