# Documentation

Start with [setup](SETUP.md) to connect a controller, reserve native DualSense
users and assign each gamepad to its own PS5 user.

| Guide | Use it for |
|---|---|
| [Setup and troubleshooting](SETUP.md) | Installation, dashboard controls, multiplayer and diagnostic reports |
| [Controller compatibility](CONTROLLERS.md) | Tested combinations, driver coverage and limits |
| [Xbox Wireless](XBOX.md) | Microsoft USB adapter pairing, bundled firmware and optional manual overrides |
| [Controller mappings](MAPPINGS.md) | USB/Bluetooth mapping profiles and raw report layouts |
| [Console storage](STORAGE.md) | Saved files, legacy migration, backups and recovery |
| [Development](DEVELOPMENT.md) | Builds, tests, module integration and release packaging |
| [Release history](releases/README.md) | Changes and requirements of older builds |

## Source builds and published downloads

These guides describe the current source on `main`. The latest published ELF
can lag behind it. Bundled Xbox adapter firmware is merged into `main` but has
not yet been published in a release. Build from source to use it now, or follow
the [manual firmware instructions](XBOX.md#advanced) with an older ELF.

Builds and host tests pass for bundling; its cold-start path still needs a PS5
check. This does not expand the hardware-tested controller list.

An older dashboard asking for one-time firmware setup needs a manual install.
An existing working install continues to work; it becomes an override when
running a bundled-firmware build. Release notes describe their own version's
behavior, including setup ZIPs that older releases shipped.
