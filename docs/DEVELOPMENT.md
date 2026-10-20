# Development

Run commands from the repository root. Build recipes live in `config/build/`;
`Makefile` is the entry point.

## Repository layout

| Path | Contents |
|---|---|
| `src/` | Payload, controller routing, USB, Bluetooth, Xbox and dashboard server |
| `vendor/anypad/`, `vendor/xow/` | Pinned protocol cores with their licenses and notices |
| `vendor/xbox/FW_ACC_00U.bin` | Verified Microsoft adapter firmware embedded at build time |
| `web/src/` | Vue components, composable state, API helpers and Tailwind styles |
| `web/public/`, `web/tests/` | Favicon and Vue interaction tests |
| `web/dist/` | Committed offline dashboard bundle with a source manifest |
| `tests/` | Host regression suites; Bluetooth and Xbox have subdirectories |
| `tools/` | Asset generators, checks and release packaging |
| `tools/xbox/` | Portable builder, probes and optional manual firmware helpers |
| `config/payload/`, `config/maps/` | Payload Manager metadata and example generic HID maps |
| `docs/` | User guides, references and historical release notes |
| `build/`, `dist/` | Ignored generated headers, objects and compiled outputs |

## Build the payload

With PS5 Payload SDK v0.43 installed and `PS5_PAYLOAD_SDK` set:

| Command | Output |
|---|---|
| `make` | Combined USB, Xbox Wireless and opt-in Bluetooth ELF in `dist/LibrePadPS5.elf` |
| `make WITH_XBOX=0` | Same filename, USB/Bluetooth only; no Xbox backend or firmware blob |
| `make xbox` | Standalone radio diagnostic ELF in `dist/LibrePadPS5-Xbox.elf` |
| `make xbox-module` | Static Xbox module in `dist/liblibrepad_xbox.a` |

A disposable Linux builder includes the SDK and compiler:

```sh
docker build -t librepadps5-builder config/build
docker run --rm -v "$PWD:/work" -w /work librepadps5-builder
```

In PowerShell use `"${PWD}:/work"` for the volume. On Windows or Unix with LLVM
and a local SDK, the portable builder also produces the combined payload:

```sh
python tools/xbox/build.py --combined --sdk /path/to/ps5-payload-sdk --clang /path/to/clang
```

Omit `--combined` for standalone diagnostics, or pass `--module-only` for the
archive. `--radio-debug` adds radio packet diagnostics. Use the combined ELF
for everyday controller use.

### Generated assets and firmware

Both build paths embed the committed dashboard bundle from `web/dist/` and
`vendor/xbox/FW_ACC_00U.bin`. The firmware generator checks the pinned SHA-256,
size and memory lengths before emitting `build/generated/xbox/firmware_blob.h`.
The Python embedder checks the dashboard source manifest before generating C
assets; stale bundles stop the build with an npm rebuild instruction. Edit
sources, not generated headers. Building the payload from a current checkout
needs no npm install or firmware download.

Embedding does not initialize the radio at app startup. The backend waits for
adapter `045e:02e6` or `045e:02fe`, then loads either a console override or the
bundled bytes. Only that adapter receives firmware transfers. The override
rules and resident-firmware reset behavior are in [Xbox firmware](XBOX.md#firmware).

### Integrate the Xbox module

The [C interface](../src/xbox/librepad_xbox.h) exposes start, poll, status, reload
and stop. Link the archive with the shared controller router, virtual-pad layer,
logging and SDK C++ runtime. Start after virtual-pad initialization, poll on the
controller loop and stop before virtual-pad cleanup. Reload queues a restart
for poll to execute; it must not join the radio worker from the HTTP thread.
The archive includes firmware and excludes the standalone USB/Bluetooth glue.

## Run checks

```sh
make check
make test
make dashboard-test
```

`make check` checks punctuation, local documentation links and active includes.
`make test` runs the C/C++ payload, Bluetooth and Xbox suites with ASan/UBSan.
These tests mock hardware; they do not establish PS5 compatibility. Some write
under the console-style state directory, so run them in the disposable builder:

```sh
docker run --rm -v "$PWD:/work" -w /work librepadps5-builder make test
```

`make dashboard-test` runs the Vue interaction suite and verifies the embedded
bundle. Install frontend dependencies as described below. The supplied Docker
image does not need Node.js for payload builds or C/C++ tests.

For the Xbox suites alone:

```sh
python tests/xbox/run.py --clang /path/to/clang++ --firmware vendor/xbox/FW_ACC_00U.bin --sanitize
```

Coverage includes adapter filtering, exact bundled bytes, override validation,
cold upload, warm restart, USB transfers, controller input and lifecycle.
`python tests/test_xbox_setup_windows.py` separately tests the optional helper
with a real Microsoft download and a disposable localhost HTTP server. It never
contacts a console. The helper itself needs Windows, not Python.

## Dashboard development

The dashboard uses Vue 3, Tailwind CSS and Vite. Its scripts, styles and favicon
are compiled locally and served by the payload; browsers need no CDN or
internet access. Use Node.js 24 for frontend development:

```sh
npm --prefix web ci
npm --prefix web run dev
npm --prefix web test
npm --prefix web run build
```

The dev server prints its localhost URL. Add `?preview` for a local fixture with
two controllers; all preview commands stay in the browser and never control a
console. Without that parameter, `/api` requests proxy to `http://127.0.0.1:8096`.
Set `LIBREPAD_PS5_URL=http://<PS5_IP>:8096` before starting Vite to use a real PS5.
For PowerShell, use `$env:LIBREPAD_PS5_URL = "http://<PS5_IP>:8096"`.
Actions in that live mode affect the console.

Components are in `web/src/components/`, polling and command state in
`composables/useDashboard.js`, and protocol helpers in `lib/`. The API routes,
control header and per-slot behavior remain compatible with the C server.
Dashboard translations are in `locales/messages.js`; each row contains the English
source and all six translations. Keep interpolation placeholders identical in every
language. Language detection, persistence and English fallback are managed by
`composables/useI18n.js`.
Interaction tests in `web/tests/` mock the API and cover native-profile
reservations, slot actions, offline recovery, uploads, mappings and stop behavior.
The C asset test also checks the compiled bytes, including UTF-8 and JavaScript
question marks that could otherwise be interpreted as C trigraphs.

After changing frontend sources, rebuild and commit `web/dist/` together with
the source and lockfile. The build manifest catches stale or missing assets;
line endings are normalized for Windows/Linux checkouts. Do not edit `dist/`
by hand. See [third-party notices](../web/THIRD_PARTY_NOTICES.md) for Vue and Tailwind.

## Share diagnostic builds in issues

Build with a distinct `--version`, for example `--version local-test`.
This changes startup/dashboard identification without editing the default in
`src/core/version.h`. Do not label modified test binaries as a stable release.

Diagnostic builds belong in the relevant issue, not GitHub Releases. Share a
ZIP with the ELF, matching test-version JSON, SHA-256 checksum and a short note
stating the source commit, purpose, steps and behavior needing console confirmation.
Copy metadata into an ignored staging directory before editing it. Remove its
stable-release `url` unless you have a matching test download URL, so it cannot
fetch a different binary. Attach the ZIP through the issue comment editor;
keep diagnostic binaries out of `main`.

The retired DS3 build notes are [historical records](releases/README.md), not
instructions to run those builds again.

## Stable release assets

Update `src/core/version.h` and `config/payload/LibrePadPS5.elf.json` to the same
stable version, including the metadata download URL, then build the combined
ELF. Stage assets with:

```sh
python tools/package_release.py
```

The packager checks the ELF's version, embedded firmware and metadata, then
writes `dist/releases/v<VERSION>/`. The directory must be empty; use
`--output /path/to/empty-directory` when restaging. It does not publish, tag or
upload anything.

Publish exactly `LibrePadPS5.elf`, `LibrePadPS5.elf.json` and
`LibrePadPS5.elf.sha256`. Do not package an Xbox setup ZIP or the manual scripts.
Keep those scripts in `tools/xbox/`. Update the release notes and any
[source-versus-release notice](README.md#source-builds-and-published-downloads)
when a source-only feature reaches a published release. Check the published
assets against the staged checksums.

## Storage and compatibility

Active console paths are in [paths.h](../src/core/paths.h); legacy migration and
both instance locks are confined to [storage.c](../src/core/storage.c).
Migration preserves a rollback snapshot and does not require symbolic links or
`realpath`. See [storage and recovery](STORAGE.md) before changing that logic.
The dashboard uses `X-LibrePad-Control`; old `X-FreePad-Control` and
`X-PDP-Control` headers retain the same strict value checks.

Add runtime code under `src/`, tests under `tests/`, tools under `tools/` and
third-party notices under `vendor/`. Keep controller coverage claims in
[CONTROLLERS.md](CONTROLLERS.md) tied to actual hardware results.
