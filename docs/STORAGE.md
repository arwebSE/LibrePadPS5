# Console storage

**v1.1.3 and newer** use **`/data/LibrePadPS5`**. v1.1.1-preview and earlier use
`/data/pdp-pad`. Stop the previous payload before upgrading; do not delete your
settings or manually move folders to work around a startup failure.

**v1.1.2 regression:** its migration requires symbolic links, which the tested
PS5 `/data` filesystem rejects. Startup can therefore fail before the dashboard
or log file opens. v1.1.3 copies files and holds both instance locks;
it does not create symbolic links.

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
| `.storage-migration` | Internal migration/recovery journal |

## Automatic migration

On the first startup after upgrading, the payload locks the legacy directory,
then the current directory. Existing firmware, mappings, profile reservations,
Bluetooth bonds, pause files and other settings are copied to the new location.
`pdp-user` becomes `controller-user`, and `pdp-pad.log` becomes `LibrePadPS5.log`.
Original files remain in the legacy directory for rollback. A running older
payload prevents migration, and the new payload keeps both locks until shutdown.

Copies use a temporary file and are renamed into place only after a complete
write. The journal allows an interrupted migration to resume. Existing files
must match the source; different settings are never silently overwritten. If
v1.1.2 already moved the old directory before failing, the fix preserves a
rollback copy and finishes renaming the current files without creating links.

After migration, new changes use the current directory. The legacy files are a
rollback snapshot; changes are not mirrored between versions. An empty legacy
directory may be kept solely to lock out older payloads. Existing valid directory
aliases from filesystems that support them remain compatible.

Unrelated settings in both directories, conflicting filenames or a damaged
journal stop startup with the operation, path and error number. Back up both
copies before resolving a conflict. Do not discard native-profile reservations.

Runtime paths are defined in [paths.h](../src/core/paths.h); migration and the
legacy lock are confined to [storage.c](../src/core/storage.c). The Xbox build
uses that same firmware path. Controller support and the dashboard are unchanged.
