# Release history

Each file records a particular build. Its installation instructions, download
assets, test status and open-issue references describe that build at the time.
Use [current setup](../SETUP.md) and [controller compatibility](../CONTROLLERS.md)
for present guidance. Published downloads are on the [Releases page](https://github.com/arwebSE/LibrePadPS5/releases).

Older releases required a separate Xbox firmware install and some shipped an
Xbox setup ZIP. Current source embeds that firmware; future standard releases
contain the ELF, Payload Manager JSON and checksum. The manual helpers remain
in `tools/xbox/`. See [source versus release availability](../README.md#source-builds-and-published-downloads).

The `v1.1.4-ds3-test.*` files preserve the DS3 investigation. Those builds are
retired; the [confirmed DS3 fix](v1.1.4.md) is in the stable release. New issue
test builds use diagnostic ZIPs shared in the relevant issue, as described in
[development](../DEVELOPMENT.md#share-diagnostic-builds-in-issues). Those bundles
serve a different purpose from the discontinued Xbox setup ZIP.
