## 0.4.5-alpha.9

- Replaced one-off private-header downloads with Endstone's Conan dependency graph.
- Added the public Endstone Cloudsmith Conan remote for the patched `raknet/4.081-mojang` recipe.
- Added Boost, EnTT, GLM, magic_enum, Microsoft GSL, base64, fmt, expected-lite and RakNet package wiring.
- Added Conan profile/toolchain generation for Clang 18/libc++ on Linux and clang-cl/lld-link on Windows.
- Kept exact BDS 1.26.32 and 1.26.33 builds isolated and diagnostic.

## 0.4.5-alpha.9

- Fixed the Endstone plugin macro inheritance error by removing `final` from plugin classes.
- Switched exact Windows builds to clang-cl, lld-link and Ninja inside the Visual Studio 2022 developer environment.
- Added the exact RakNet header source required by Endstone private Bedrock headers.
- Synchronized workflow, package, CMake and source-release versions.
- Included the actual hidden `.github/workflows/ci.yml` files in the release package.

# Changelog

## 0.4.5-alpha.9

### Fixed

- Fixed Linux exact-build exit code 126 by invoking `scripts/build_exact.sh` through `bash`; the build no longer depends on Git preserving an executable bit.
- Moved Linux exact builds to Ubuntu 24.04 with explicit Clang 18, libc++ 18 and libc++abi 18.
- Replaced the unsupported Windows `clang-cl`/Ninja exact build with Visual Studio 2022 and MSVC x64.
- Added native-command exit checks to the PowerShell build script.
- Added failed-build diagnostic artifacts containing CMake logs and cache data.
- Hardened exact-build version/platform validation and release checksum generation.

## 0.4.5-alpha.9

- Added deterministic CMake install layouts for exact native builds.
- Added stable BDS- and platform-specific plugin filenames.
- Added raw `.dll`/`.so`, complete ZIP package, package manifest, and SHA-256 generation.
- Added GitHub Actions artifact uploads for every exact Windows and Linux build.
- Added automatic GitHub Release publishing when tag `v0.4.5-alpha.9` is pushed.
- Added release-tag validation and repeatable release asset replacement.
- Removed the unpinned Windows Ninja action and install Ninja through Python instead.
- Added package-integrity verification before artifacts are uploaded.

## 0.4.5-alpha.9

- Added exact `ChunkSource::createNewChunk` and `getOrLoadChunk` interception for the 26.30-family adapter.
- Added bounded per-dimension request queues, sequence IDs, deduplication, capture retries, and backpressure metrics.
- Connected real chunk requests to detached worker population and primary-thread commit.
- Added changed-block-only commits and `flushThreadBatch()` after successful commits.
- Registered the live API as Endstone service `endstone:worldgen`.
- Added a native populator registration example.
- Added a mock interception/dispatch/commit C++ test.
- Kept Mojang's hidden base-terrain generator off foreign threads.

## 0.2.0-alpha.2

- Added exact Minecraft Bedrock 26.30-family runtime gate.
- Pinned BDS 1.26.32 to Endstone v0.11.5 and BDS 1.26.33 to Endstone v0.11.6.
- Added block palettes to detached chunk buffers.
- Added exact detached full-chunk capture and controlled primary-thread commit.
- Added BDS ChunkSource diagnostics, `canLaunchTasks()` visibility, and `flushThreadBatch()` bridge.
- Added exact-build shell and PowerShell build scripts.
- Added Windows/Linux GitHub Actions source-build matrix for both supported BDS builds.
- Left direct vanilla generation-stage interception disabled until live BDS safety validation is complete.

## 0.1.0-alpha.1

- Initial detached world-generation SDK, worker pool, custom generators, populators, and native adapter boundary.
