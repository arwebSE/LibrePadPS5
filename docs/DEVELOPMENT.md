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

`make test` runs all 21 C/C++ host suites with sanitizers, including storage migration and the PS5 USB
reader's initialization ordering and interleaved input/output completions. Tests create files under
the state directory, so use the disposable builder, not a console.
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

For a separately identified test payload, pass `--version` to the portable
builder. This overrides the startup and diagnostic version without changing the
default version in `src/core/version.h`:

```sh
python tools/xbox/build.py --combined --sdk /path/to/ps5-payload-sdk --version 1.1.4-ds3-test.1
```

Use matching test-version metadata when distributing that ELF through Payload
Manager. Keep stable release metadata in `config/payload/` unchanged.

## Share diagnostic builds in issues

Diagnostic builds belong in the relevant GitHub issue, not in GitHub Releases.
Do not create releases or release tags for controller experiments, retries or
individual tester builds. Keep the normal download pointing to the
[latest stable ELF](https://github.com/arwebSE/LibrePadPS5/releases/latest/download/LibrePadPS5.elf).

Build with a distinct `--version`, then package a ZIP containing:

- `LibrePadPS5.elf` from that build.
- `LibrePadPS5.elf.json` with the same test version. Copy the stable metadata
  into the ignored build directory before editing it; do not change the stable
  metadata. Remove its stable-release `url` until an actual test attachment URL
  exists, so the test metadata cannot download a different ELF.
- `LibrePadPS5.elf.sha256`, calculated from the packaged ELF.
- A short note with the source commit, relevant issue, purpose, instructions
  and checks performed. State which behavior still needs console confirmation.

Use an identifiable filename such as `LibrePadPS5-issue-4-ds3-test-6.zip`.
Attach the ZIP in the issue's comment editor and post the test instructions
with it. GitHub accepts ZIP attachments; an ELF should be placed inside the ZIP.
An ordinary source commit is sufficient; distributing a test does not require
a release or tag. See [GitHub attachment instructions](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/attaching-files).

For distribution using GitHub CLI only, GitHub's attachment endpoint currently
rejects ZIP files. Store the ZIP on a separate `codex/issue-<number>-test-builds`
branch and post an immutable raw download link in the issue. This is an
issue-linked download, not a native attachment. Keep binary test bundles out
of `main`; do not create a release as a workaround.

DS3 tests 1 through 5 have been retired from release downloads. The test 1 page
still points to the stable build because GitHub's recent page-traffic report
includes it; the other four test release pages were deleted. Source tags and
diagnostic history remain available. This retirement does not establish that
DS3 input is fixed.

## Compatibility and naming

New code uses LibrePad names. Active console paths are defined in
[paths.h](../src/core/paths.h). [Storage migration](STORAGE.md) copies settings
to `/data/LibrePadPS5`, retains a legacy rollback snapshot and locks both namespaces.
The tested PS5 filesystem rejects symbolic links. SDK v0.43's raw libc stubs also
leave misleading errno values on failure; see the [libc stubs](https://github.com/ps5-payload-dev/sdk/blob/v0.43/libc/syscalls.c)
and [CRT gateway](https://github.com/ps5-payload-dev/sdk/blob/v0.43/crt/syscall.c).
Do not require symbolic links or `realpath` for startup. Storage tests prohibit
those calls and cover copy recovery, both locks, rollback and conflicting files.
Startup failures report the exact operation/path and errno before file logging.
The dashboard sends `X-LibrePad-Control`; the server also accepts the released
`X-FreePad-Control` and `X-PDP-Control` headers with the same strict value checks.

Add runtime code under `src/`, tests under `tests/`, and tools under `tools/`.
Keep third-party code and notices under `vendor/`. Edit `web/` rather than its
generated C assets. Historical source tags remain available; retired diagnostic
release pages point to the stable download and no longer host test binaries.
