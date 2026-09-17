# Validation results

## BDS 1.26.51 / Endstone 0.11.11 target

The active compatibility target is **game 1.26.51**, server package
**1.26.51.1**, and **Endstone 0.11.11**. Source command wheels require
**Endstone >=0.11.11**, with no upper bound. Both official server archives
have been checksum-verified and the matching SDK commit is recorded.
The native adapter port and live validation remain pending.
See [target details and verification commands](docs/BDS_1_26_51.md).
The release information below describes the previous exact server target.



Validated on 2026-07-28:

- Portable MSVC Release configuration and C++20 build
- CTest WorldGen suite (3/3)
- Python unit, release-tool, metadata, native source-guard, command, form-navigation, live-bridge, strict logger, and bridge-loader tests (43/43; 37 command-route subtests)
- Platform-wheel contracts for entry points, native-plugin dependencies, commands,
  permissions, CPython 3.14 tags, package-local bridges, binary magic, and RECORD integrity
- Project/version/dependency metadata consistency for `0.4.7`
- GitHub Actions YAML parsing
- Release packaging round-trip with a synthetic Windows plugin stage
- Checksum, ZIP path, manifest, native bridge, unresolved Bedrock symbol,
  CPython 3.14 SOABI, dynamic runtime dependency, and non-relocatable RPATH
  rejection gates
- Stable release filenames for BDS 1.26.33 on Linux and Windows
- Player-height anchor propagation, changed-Y range validation, the exact 12
  registered `/wg` usages, and every command overload
- Empty automatic pipelines, no-op commit suppression, bounded recapture retries,
  changed-cell conflict rejection, new-palette and biome-capability merges,
  preservation of unrelated live edits, exact loaded-state/block-actor/write-readback
  source guards, and visibly labeled menu defaults
- Source ZIP integrity and `git diff --check`

Not validated in this environment:

- Exact Endstone/Bedrock native builds and relocated CPython 3.14 wheel imports
  from the GitHub Actions Linux and Windows matrix
- Loading the exact native `.dll` or `.so` inside BDS 1.26.33
- Live `ChunkSource` interception against a production world
