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

The latest release includes the multilingual Vue dashboard, bundled Xbox adapter firmware
and automatic Xbox reconnection after service restarts. Dashboard languages follow
browser preferences, with English as the fallback and a saved language menu choice.
These guides describe the current source on `main`; release notes record changes
in each published build.

Bundled firmware cold startup and Xbox One model 1697 input are confirmed on
PS5 12.60. See [controller coverage](CONTROLLERS.md) for other tested combinations.

An older dashboard asking for one-time firmware setup needs a manual install.
An existing working install continues to work; it becomes an override when
running a bundled-firmware build. Release notes describe their own version's
behavior, including setup ZIPs that older releases shipped.
