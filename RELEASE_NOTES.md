# v0.4.12 - Minecraft 26.60 / Endstone 0.11.15

Target BDS **1.26.60.6**, runtime **26.60**, protocol **2225**, and CPython **3.14** on Linux x86-64. The exact SDK is pinned to `b4aaa87d2c5305e7d340168f824ac2e2ea722e44`. Native binaries and matching wheels passed automated live checks, saved-block restart, and clean shutdown. See `compatibility/native-qualification.json` for artifact hashes and test scope.

Use the default `enable-script=true`. The disabled-scripting configuration and Windows native API packages are unqualified. No connected-player session was used for this update. Historical notes follow.

---

> Endstone and BDS release update v0.4.11. BDS is 1.26.52.3.

# Endstone WorldGen API v0.4.11

Linux x86-64 native release for Minecraft **1.26.52**, BDS **1.26.52.3**, and Endstone **0.11.14**, with matching CPython **3.14** command wheels.

Endstone package metadata accepts **>=0.11.11** with no upper bound. Native hooks require the verified BDS 1.26.52.3 / Endstone 0.11.14 binary pair; later private runtimes need separate qualification.

Includes the native Linux `.so`, matching command wheel, deployment archive, source/SDK assets, and SHA-256 checksums.

Verified against the supplied server files using isolated live servers, native C++ tests, Python tests, exact binary guards, and clean shutdown checks. See `compatibility/native-qualification.json` for retained live evidence. No server binaries are redistributed.
