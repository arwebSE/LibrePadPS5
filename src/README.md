# Source map

```text
app/        Payload startup and shutdown
core/       Controller state, logging, timing, legacy storage paths
input/      USB discovery/reading, shared source slots and report decoding
bluetooth/  Queued Classic/LE host adapter; disabled until explicitly enabled
ps5/        Virtual-controller ownership, user binding, system credentials
dashboard/  HTTP endpoints; generated interface assets come from web/
legacy/     Archived reference files, not compiled into the payload
```

Start at `app/main.c`. Add report formats in `input/controller_profiles.c`;
Edit HTML, CSS and JavaScript in the repository's `web/` directory.
USB transport lives in `input/usb_device.c`, not in a decoder. Console-user
protection and owned-device cleanup live in `ps5/virtual_controller.c`.

Dedicated USB decoders live in `input/protocol_decoder.c`. Bluetooth and
descriptor-based generic HID use a pinned core in `vendor/anypad/`, behind
the owned-device adapter. The optional Xbox radio is behind a build flag and four-function C interface. No remote shell injection or
native-handle recycling is present. Platform guards support PS5 compilation
and host tests. Address approval and ownership checks are intentional.

Use plain punctuation throughout the project, never em dashes.
The punctuation check in `make test` enforces this.
