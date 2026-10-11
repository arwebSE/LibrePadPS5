# Console storage

**v1.1.1 and newer** use **`/data/LibrePadPS5`**.
v1.1.1-preview and earlier releases use `/data/pdp-pad`.
Stop the previous payload before loading a new build. No manual file moves are needed.

| File or folder | Purpose |
|---|---|
| `LibrePadPS5.log` | Payload log; also downloadable from the dashboard |
| `native-user`, `native-users` | Reserved DualSense profiles |
| `controller-user` | Optional preferred profile for the first gamepad |
| `controllers.conf`, `maps/` | Saved controller layouts and button mappings |
| `bluetooth.db` | Bluetooth bonds |
| `xbox/firmware.bin` | Xbox Wireless Adapter firmware |
| `pause`, `pause-N` | Saved gamepad release state |
| `stop` | Request a clean shutdown |
| `instance.lock` | Prevent simultaneous payload instances |

## Automatic migration

On the first startup after upgrading, the payload locks the existing instance
file, then renames the entire old directory to `/data/LibrePadPS5`. This preserves
firmware, mappings, profile reservations, Bluetooth bonds, pause files and any
other files already in that directory. It also renames `pdp-user` to
`controller-user` and `pdp-pad.log` to `LibrePadPS5.log`.

The old directory and filenames become symlinks to the new locations. They are
compatibility aliases, not separate storage. Older payloads therefore reach
the same settings and instance lock. A running older payload prevents migration.
The migration does not delete or overwrite settings. If interrupted after the
directory move, the next startup can finish creating the aliases.

If both directory names are real directories, or both old and new filenames
already contain separate files, startup refuses to choose between them. Stop
all controller payloads, back up both copies through FTP, and resolve the
conflict before retrying. Do not discard native-profile reservations.

Runtime components use the paths in [paths.h](../src/core/paths.h). The old names
are confined to [storage.c](../src/core/storage.c) and compatibility documentation.
The Xbox build uses that same firmware path rather than its own hardcoded copy.
