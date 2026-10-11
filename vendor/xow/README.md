# xow radio and GIP core

Source: [medusalix/xow](https://github.com/medusalix/xow), commit
`d335d6024f8380f52767a7de67727d9b2f867871`.

GPL-2.0-or-later, with the upstream ISC notices retained in MT76 code. Original
copyright and [license text](LICENSE) remain intact. Only the radio/protocol
source used by LibrePadPS5 is included.

PS5 USB, controller-sink, logging and firmware adapters live in `src/xbox/`.
The small forwarding headers here bind the upstream core to those adapters.
See [source provenance and local changes](../../docs/XBOX.md#source-provenance).
