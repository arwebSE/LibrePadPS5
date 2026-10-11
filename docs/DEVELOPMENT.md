# Development and repository layout

Run commands from the repository root. `Makefile` is the entry point; build
recipes are in `config/build/`. The root keeps the README, license, Makefile and
Git configuration alongside the project directories.

| Path | Contents |
|---|---|
| `src/` | Active payload code; Xbox Wireless is in `src/xbox/` |
| `vendor/anypad/`, `vendor/xow/` | Pinned third-party cores with licenses and attribution |
| `web/` | Editable dashboard HTML, CSS, JavaScript and icons |
| `tests/` | Host regression suites; Bluetooth and Xbox have their own subdirectories |
| `tools/` | Dashboard embedding and repository checks; Xbox builders/probes are in `tools/xbox/` |
| `config/build/` | Payload, Xbox and test make recipes, plus the Dockerfile |
| `config/payload/` | Payload Manager metadata |
| `config/maps/` | Example generic HID mapping files |
| `docs/` | Setup, controller coverage, Xbox guide, development guide and release notes |
| `docs/reference/omnipad/` | Archived source, tests and TCP tool; excluded from current builds/checks |
| `build/`, `dist/` | Ignored generated files and compiled outputs |

## Build and test

With PS5 Payload SDK v0.43 installed and `PS5_PAYLOAD_SDK` set:

```sh
make
make WITH_XBOX=0
make xbox
make xbox-module
```

For a disposable Linux builder:

```sh
docker build -t librepadps5-builder config/build
docker run --rm -v "$PWD:/work" -w /work librepadps5-builder
docker run --rm -v "$PWD:/work" -w /work librepadps5-builder make test
```

In PowerShell, use `"${PWD}:/work"` for the volume. The main output is
`dist/LibrePadPS5.elf`. Install it with the metadata from
[config/payload](../config/payload/LibrePadPS5.elf.json).

`make test` runs all 20 C/C++ host suites with sanitizers, including the PS5 USB
reader's initialization ordering and interleaved input/output completions. Tests create files under
the legacy state directory, so use the disposable builder, not a console.
`make dashboard-test` runs the JavaScript checks and requires Node.js.
`make check` validates punctuation, local documentation links and active C/C++
includes. `make help` lists the entry points.

On Windows, `python tests/test_xbox_setup_windows.py` exercises the setup helper
with a real Microsoft download, native CAB extraction, hash verification and a
disposable localhost HTTP server. It never contacts a console. Python is only
needed for this integration test; the shipped setup helper uses Windows alone.

The portable LLVM builder is [tools/xbox/build.py](../tools/xbox/build.py).
It supports combined, standalone and module-only builds. See [Xbox setup and
verification](XBOX.md) for arguments and firmware staging.

## Compatibility and naming

New code uses LibrePad names. Existing console storage paths are defined in
[legacy_paths.h](../src/core/legacy_paths.h) and stay unchanged so saved profiles,
native-user protection, pause flags and the instance lock remain compatible.
The dashboard sends `X-LibrePad-Control`; the server also accepts the released
`X-FreePad-Control` and `X-PDP-Control` headers with the same strict value checks.

Add runtime code under `src/`, tests under `tests/`, and tools under `tools/`.
Keep third-party code and notices under `vendor/`. Edit `web/` rather than its
generated C assets. Historical release binaries remain attached to their tags.
