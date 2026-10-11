# Source map

```text
app/        Payload startup and shutdown
core/       Controller state, logging, timing, legacy storage paths
input/      USB discovery/reading and configurable report decoding
ps5/        Virtual-controller ownership, user binding, system credentials
dashboard/  HTTP endpoints; generated interface assets come from web/
legacy/     Archived code; the old decoder is used only for regression tests
```

Start at `app/main.c`. Add report formats in `input/controller_profiles.c`;
Edit HTML, CSS and JavaScript in the repository's `web/` directory.
USB transport lives in `input/usb_device.c`, not in a decoder. Console-user
protection and owned-device cleanup live in `ps5/virtual_controller.c`.

The active build has no OmniPad feature mode: Bluetooth, generic fallback,
remote shell injection and native-handle recycling are excluded by source,
not by a build flag. The remaining platform guards support PS5 compilation
and host tests. Runtime validation is intentional and must not be removed.
