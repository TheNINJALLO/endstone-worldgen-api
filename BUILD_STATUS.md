# Build status

## BDS 1.26.51 / Endstone 0.11.11 target

The active compatibility target is **game 1.26.51**, server package
**1.26.51.1**, and **Endstone 0.11.11**. Source command wheels require
**Endstone >=0.11.11**, with no upper bound. Both official server archives
have been checksum-verified and the matching SDK commit is recorded.
The native adapter port and live validation remain pending.
See [target details and verification commands](docs/BDS_1_26_51.md).
The release information below describes the previous exact server target.



Version: **0.4.8-alpha.1**

## Implemented

- Portable C++ WorldGen core and tests
- Python package and tests
- Exact BDS 1.26.33 / Endstone v0.11.6 adapter source
- Native `ChunkSource` request interception pipeline
- Detached worker processing and primary-thread commits
- Deterministic native install and packaging scripts
- GitHub Actions Windows x64 and Linux x64 exact builds for BDS 1.26.33
- Downloadable workflow artifacts on every push
- Automatic tagged GitHub Releases
- Raw plugin, ZIP package, manifest, and SHA-256 outputs
- Verified CPython 3.14 platform command wheels with a bundled native status bridge
- Build-time rejection of unresolved Bedrock ABI symbols and release-time RPATH validation
- ABI-versioned `endstone:worldgen:v2` service with primary-thread live recipes
- Commit-and-flush-confirmed `/wg gen` and `/wg structure` command paths anchored
  at the block below the sender, with exact native changed-Y reporting
- Exact loaded-state and block-actor preflight plus public virtual runtime-ID readback verification
- Conflict-safe automatic-populator block/biome delta merge with bounded recapture retries
- Explicit live-write/detached menu labels with a visible maze default

## Validation boundary

Portable builds and package tooling are validated locally. Exact native binaries are compiled by the included GitHub Actions runners and still require first-load testing against the matching BDS executable before production use.

## GitHub Actions toolchain hotfix

- Linux exact builds run on Ubuntu 22.04 with Clang 18 and libc++ 18.
- Both platforms invoke `scripts/build_exact.py`, so executable-bit loss cannot cause exit code 126.
- Windows exact builds use clang-cl, lld-link, and Ninja inside the Visual Studio 2022 developer environment.
- Failed exact jobs upload CMake diagnostics for inspection.
