# Endstone WorldGen API Wiki & Technical Manual

Welcome to the official documentation and technical wiki for **Endstone WorldGen API**.

This library provides multi-threaded terrain chunk interception, asynchronous generation scheduling, and cross-chunk neighborhood boundary locking for **Endstone Minecraft Bedrock Edition** servers.

---

## 🎯 Quick Navigation

- [🏗️ Architecture & Multi-Threading Model](ARCHITECTURE.md) — Learn about `GenerationScheduler` worker threadpools, uncompressed 3D `ChunkBuffer` memory layout, and canonical FNV-1a fingerprint verification.
- [⚙️ Custom Generators & Pipeline Stages](generators_and_stages.md) — Implement custom terrain generators, noise functions, and pipeline stages (`BIOMES`, `BASE_TERRAIN`, `SURFACE`, `CARVERS`, `STRUCTURES`).
- [🔒 Neighborhood Boundary Lock Engine](neighborhood_locking.md) — Synchronize multi-chunk structure generation across 3x3 chunk boundaries without race conditions or edge tears.
- [🎮 In-Game Command Suite Guide](in_game_testing.md) — Full reference for the `/wg` command suite and packaged studio plugin wheel.
- [📘 Complete API Reference](api_reference.md) — Comprehensive class, interface, and method reference for C++ and Python.

---

## ⚡ Basic Example

```python
from endstone_worldgen import (
    GenerationScheduler,
    GenerationContext,
    ChunkPos,
    FlatGenerator,
)

scheduler = GenerationScheduler(workers=4)
ctx = GenerationContext(world_seed=12345, dimension="overworld", chunk=ChunkPos(0, 0))

future = scheduler.generate(ctx, FlatGenerator(surface_y=64, base=1, top=2))
buf = future.result()

print(f"Chunk (0,0) Fingerprint: {hex(buf.fingerprint())}")
print(f"Surface block at Y=64: {buf.get(0, 64, 0)}")

scheduler.close()
```
