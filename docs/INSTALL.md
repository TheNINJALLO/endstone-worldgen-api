# Installation

## Downloading an automatic build

Every GitHub push produces downloadable Windows x64 and Linux x64 artifacts for BDS 1.26.32 and 1.26.33. Open the repository's **Actions** tab, select the completed build, and download the package matching your exact BDS version and operating system.

A tagged release such as `v0.4.5-alpha.9` publishes the same files under the repository's **Releases** page.

Copy the packaged plugin from `plugins/` into Endstone's native plugin directory.

## Building locally

Linux:

```bash
./scripts/build_exact.sh 1.26.33 linux-x64
```

Windows PowerShell:

```powershell
./scripts/build_exact.ps1 -BdsBuild 1.26.33 -Platform windows-x64
```

Completed raw plugins, ZIP packages, and checksums are written to `dist/release/`.

The plugin refuses non-matching BDS builds, installs the exact `ChunkSource` request hooks, registers `endstone:worldgen`, and starts its primary-thread pump. The hook has no useful world-editing work until at least one native plugin registers an `IPopulator`; see `examples/cpp/parallel_ore_populator.cpp`.

## Native build boundary

The exact adapters include Endstone's private BDS declarations and must be compiled with the matching Endstone source tag and ABI toolchain. The portable test build does not certify the native adapter. Treat the first live load as a staging test, keep a world backup, and confirm the startup capability log before allowing generation commits.
