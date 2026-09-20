# Build status

Version: **0.4.9**

Release target: Endstone **0.11.12**, BDS package
1.26.51.1/runtime 26.51, and CPython 3.14 on Linux x86-64.

Pinned SDK commit: `1c71186cba896c5e0bc432384a8a8e72dfb2a626`. BDS executables, archive hashes, and native BDS addresses are unchanged.

## Validation

Six generation recipes, native chunk interception, capture/commit/flush, zero unconfirmed blocks, save/resume, and shutdown.

Native C++ and Python test suites and disposable-server evidence are recorded in `compatibility/native-qualification.json`.

Endstone package metadata accepts **>=0.11.11** without an upper bound. Native hooks remain gated to the exact qualified runtime.

## Publication

Release: [v0.4.9](https://github.com/TheNINJALLO/endstone-worldgen-api/releases/tag/v0.4.9). The previous [v0.4.8 release](https://github.com/TheNINJALLO/endstone-worldgen-api/releases/tag/v0.4.8) and its 0.11.11 qualification remain historical records.
