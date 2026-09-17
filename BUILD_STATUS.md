# Build status

Version: **0.4.8**

[Native release v0.4.8](https://github.com/TheNINJALLO/endstone-worldgen-api/releases/tag/v0.4.8) targets Endstone 0.11.11, BDS package
1.26.51.1/runtime 26.51, and CPython 3.14 on Linux x86-64.

Pinned SDK commit: `37b395378d91d6d20f1c52bf9d79dbd20e152458`.

## Validation

42-entry ChunkSource vtable verified; three dimension sources hooked; all six live recipes passed capture/commit/flush checks with zero unconfirmed blocks; clean shutdown.

Native C++ and Python test suites, binary identity checks, package checks, and deployment evidence accompany the release. Enchantment additionally requires ASan/UBSan and `production_ready: true` from its live production check.

Endstone package metadata accepts **>=0.11.11** with no upper bound. Native hooks require the verified BDS 1.26.51.1 / Endstone 0.11.11 binary pair; later private runtimes need separate qualification.
