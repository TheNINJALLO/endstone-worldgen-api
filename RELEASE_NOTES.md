> Endstone and BDS release update v0.4.10. BDS is 1.26.52.3.

# Endstone WorldGen API v0.4.10

Linux x86-64 native release for Minecraft **1.26.52**, BDS **1.26.52.3**, and Endstone **0.11.13**, with matching CPython **3.14** command wheels.

Endstone package metadata accepts **>=0.11.11** with no upper bound. Native hooks require the verified BDS 1.26.52.3 / Endstone 0.11.13 binary pair; later private runtimes need separate qualification.

Includes the native Linux `.so`, matching command wheel, deployment archive, source/SDK assets, and SHA-256 checksums.

Verified against the supplied server files using isolated live servers, native C++ tests, Python tests, exact binary guards, and clean shutdown checks. See `compatibility/native-qualification.json` for retained live evidence. No server binaries are redistributed.
