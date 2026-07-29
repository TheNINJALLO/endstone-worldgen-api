# Installation

## Downloading an automatic build

Every GitHub push produces downloadable Windows x64 and Linux x64 artifacts for BDS 1.26.33 with Endstone v0.11.6. Open the repository's **Actions** tab, select the completed build, and download the package matching your operating system.

A tagged release such as `v0.4.7` publishes the same files under the repository's **Releases** page.

Use the ZIP matching the server's exact BDS build and operating system. Copy its packaged plugin from `plugins/` into Endstone's native plugin directory. Do not mix the native plugin or Python bridge from another BDS build.

The `/wg` plugin requires Endstone's **CPython 3.14** runtime. The complete ZIP contains a platform-specific `cp314` wheel with `_endstone_worldgen_live` bundled inside it. Stop the server, remove every older WorldGen Studio wheel from `plugins/` and any manually copied top-level `_endstone_worldgen_live` file from `.local`, then copy both files from the ZIP's `plugins/` directory into the server's `plugins/` directory. No `PYTHONPATH` or manual `site-packages` copy is required.

## Building locally

Linux:

```bash
./scripts/build_exact.sh 1.26.33 linux-x64
```

Windows PowerShell:

```powershell
./scripts/build_exact.ps1 -BdsBuild 1.26.33 -Platform windows-x64
```

Completed raw plugins, self-contained platform wheels, ZIP packages, and checksums are written to `dist/release/`.

The plugin refuses non-matching BDS builds, registers service ABI 2 as `endstone:worldgen:v2`, tries
to install the exact `ChunkSource` request hooks, and starts its primary-thread
pump. If hook installation fails, automatic intercepted population is disabled
but the service remains registered so its manual exact capture/commit/flush path
can still operate. Automatic interception intentionally waits for at least one
dependent native plugin to register an `IPopulator`; this normal startup state
is informational, not a load failure. See
`examples/cpp/parallel_ore_populator.cpp`. The `/wg gen` and `/wg structure`
commands use separate bounded built-in live recipes and do not require a
registered populator.

Live recipe targets must be in exact `ChunkState::Loaded`; changed cells with a
block actor are refused because the detached buffer does not carry actor NBT.
The menu defaults live generation
to `maze` because it is visibly above the floor; `flat` may resemble an existing
grass surface, `ores` is underground, and every `buffer`, `benchmark`, or
`inspect` action is detached and cannot change the server world.

## Native build boundary

The exact adapters include Endstone's private BDS declarations and must be compiled with the matching Endstone source tag and ABI toolchain. The portable test build does not certify the native adapter. Treat the first live load as a staging test, keep a world backup, and confirm the startup capability log before allowing generation commits.
