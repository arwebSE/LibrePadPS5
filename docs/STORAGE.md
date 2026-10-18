# Console storage

LibrePadPS5 keeps settings and logs in `/data/LibrePadPS5/`. Updating the ELF
replaces the app and its bundled Xbox firmware, while saved settings and manual
firmware overrides stay on the console. Normal setup needs no folder edits.

| File or folder | Purpose |
|---|---|
| `LibrePadPS5.log` | Payload log; diagnostic downloads include its recent contents |
| `native-user`, `native-users` | PS5 users reserved for native DualSense controllers |
| `controller-user` | Optional preferred PS5 user for the first virtual slot |
| `controllers.conf`, `maps/` | Driver/button profiles and generic HID mappings |
| `bluetooth.db` | Saved built-in Bluetooth bonds |
| `xbox/firmware.bin` | Optional Xbox adapter firmware override, taking priority over the ELF's copy |
| `pause`, `pause-N` | Legacy global release state and per-slot release state |
| `stop` | Empty file requesting a clean shutdown |
| `instance.lock` | Instance lock used to prevent simultaneous payloads |
| `.storage-migration` | Internal migration/recovery journal |

## Xbox firmware and saved overrides

Bundled firmware is part of the ELF; there is no separate bundled file to copy
or back up on the PS5. An older manual install at `xbox/firmware.bin` remains an
explicit override after upgrading. An invalid or unreadable override blocks
Xbox radio initialization rather than silently using the bundle. It does not
change the other controller drivers.

To return to bundled firmware, stop the payload, back up and remove only
`/data/LibrePadPS5/xbox/firmware.bin`, then reload the payload and unplug/replug
the adapter. Leave the legacy rollback directory alone. This requires a build
that includes firmware; see [Xbox setup and advanced installation](XBOX.md).

## Backups and recovery

Stop the payload before copying its settings through FTP. Keep native-user
reservations, mappings and Bluetooth bonds in the backup. Release state belongs
to virtual slots, not controller serial numbers: a released slot stays paused
across restarts until explicitly reconnected in the dashboard.

A storage startup failure reports the operation, path and error number in the
notification or loader output, even when file logging cannot start. Stop any
older payload, use a current build and preserve both storage directories before
resolving a reported conflict. Deleting a lock file does not stop its process.
Do not remove the migration journal or move folders to bypass the error.

## Automatic migration from older installations

Older payloads used `/data/pdp-pad/`. Startup locks that directory and the
current directory, then copies existing settings, manual firmware, bonds,
profiles and pause files. `pdp-user` becomes `controller-user`, and
`pdp-pad.log` becomes `LibrePadPS5.log`. Stop the older payload first.

Copies use a temporary file and rename after a complete write. The journal
allows interrupted migration to resume; conflicting files are never silently
overwritten. The migration also recovers the directory move left by the
[older startup regression](releases/v1.1.3.md), without creating symbolic links.

The original files remain a rollback snapshot. Later changes use only the
current directory and are not mirrored to that snapshot. An empty legacy
directory can remain solely to lock out older payloads; it is not a second
active settings location. Existing working directory aliases remain compatible.

Runtime paths are in [paths.h](../src/core/paths.h), with migration and legacy
locking in [storage.c](../src/core/storage.c).
