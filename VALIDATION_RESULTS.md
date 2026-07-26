# Validation results

Validated on 2026-07-26:

- Portable MSVC Release configuration and C++20 build
- CTest WorldGen suite (3/3)
- Python unit, release-tool, metadata, strict logger, and bridge-loader tests (20/20)
- Platform-wheel contracts for entry points, native-plugin dependencies, commands,
  permissions, CPython 3.14 tags, package-local bridges, binary magic, and RECORD integrity
- Project/version/dependency metadata consistency for `0.4.5-beta.31`
- GitHub Actions YAML parsing
- Release packaging round-trip with a synthetic Windows plugin stage
- Checksum, ZIP path, manifest, native bridge, unresolved Bedrock symbol,
  CPython 3.14 SOABI, dynamic runtime dependency, and non-relocatable RPATH
  rejection gates
- Stable release filenames for BDS 1.26.33 on Linux and Windows
- Source ZIP integrity and `git diff --check`

Not validated in this environment:

- Exact Endstone/Bedrock native builds and relocated CPython 3.14 wheel imports
  from the GitHub Actions Linux and Windows matrix
- Loading the exact native `.dll` or `.so` inside BDS 1.26.33
- Live `ChunkSource` interception against a production world
