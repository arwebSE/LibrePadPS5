# AnyPad Bluetooth core

Source: [AnyPad-PS5-English](https://github.com/Mohammad-Aghayan/AnyPad-PS5-English), commit `9475e9f88b7d06f890ca3bb3f16537bcc503eff4`.

GPL-3.0. Original credits and license remain applicable. Only source files are imported, not upstream Git history. `log.c` is linked only by isolated upstream regression tests; the payload uses LibrePadPS5 logging.

Local changes: namespaced clock, explicit address approval before Classic/LE connections, no unknown-handle disconnect sweep, descriptor-verified HCI access, private pairing-key file permissions, startup-failure cleanup, minimum event-length validation, and plain punctuation. The USB adapter also checks HID gamepad/joystick application collections before accepting generic devices.

The imported host changes shared HCI event masks and pairing mode. Explicit addresses reduce accidental pairing but cannot guarantee native DualSense isolation. Bluetooth hardware testing is still required.
