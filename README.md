# Endstone WorldGen API

[![Version](https://img.shields.io/badge/version-v0.4.5--alpha.9-blue.svg?style=for-the-badge)](https://github.com/TheNINJALLO/endstone-worldgen-api/releases/tag/v0.4.5-alpha.9)
[![Endstone](https://img.shields.io/badge/Endstone-v0.11.5%20%7C%20v0.11.6-emerald.svg?style=for-the-badge)](https://github.com/EndstoneMC/endstone)
[![BDS Support](https://img.shields.io/badge/BDS-1.26.32%20%7C%201.26.33-purple.svg?style=for-the-badge)](https://www.minecraft.net/en-us/download/server/bedrock)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg?style=for-the-badge)](https://github.com/TheNINJALLO/endstone-worldgen-api/actions)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20x64-orange.svg?style=for-the-badge)](#installation)
[![Language](https://img.shields.io/badge/c%2B%2B-20-blue.svg?style=for-the-badge)](#)
[![Python](https://img.shields.io/badge/python-3.9%2B-yellow.svg?style=for-the-badge)](#)
[![License](https://img.shields.io/badge/license-MIT-green.svg?style=for-the-badge)](LICENSE)

> Multi-threaded terrain chunk interceptor, asynchronous generation scheduler, and neighborhood boundary lock engine for **Endstone Minecraft Bedrock Edition** servers.

---

## 🚀 Key Features

- **⚡ Multi-Threaded Chunk Generation**: Asynchronous worker threadpool (`GenerationScheduler`) offloading heavy noise calculations from the main server loop.
- **🧱 Uncompressed 3D Chunk Buffer**: High-speed direct 3D array index access `(X, Y, Z)` with BLAKE2b fingerprint hashing.
- **🔒 Neighborhood Boundary Lock Engine**: Cross-chunk thread synchronization ensuring multi-chunk structure placement without race conditions or edge tears.
- **🌐 Dual C++20 & Python 3.9+ API**: Native C++ service registered in Endstone `ServiceManager` alongside clean Python interfaces.
- **🎮 In-Game Interactive Studio**: Includes built-in `/wg` command suite for live terrain generation, structure spawning, and stress testing.

---

## 📦 Direct Release Downloads (`v0.4.5-alpha.9`)

Download official pre-compiled binaries matching your target server OS and BDS version:

| Target Platform | BDS Version | Plugin Binary Asset | Download |
| :--- | :--- | :--- | :--- |
| **Windows x64** | `1.26.32` | `endstone-worldgen-api-v0.4.5-alpha.9-bds-1.26.32-windows-x64.dll` | [Download](https://github.com/TheNINJALLO/endstone-worldgen-api/releases/download/v0.4.5-alpha.9/endstone-worldgen-api-v0.4.5-alpha.9-bds-1.26.32-windows-x64.dll) |
| **Windows x64** | `1.26.33` | `endstone-worldgen-api-v0.4.5-alpha.9-bds-1.26.33-windows-x64.dll` | [Download](https://github.com/TheNINJALLO/endstone-worldgen-api/releases/download/v0.4.5-alpha.9/endstone-worldgen-api-v0.4.5-alpha.9-bds-1.26.33-windows-x64.dll) |
| **Linux x64** | `1.26.32` | `endstone-worldgen-api-v0.4.5-alpha.9-bds-1.26.32-linux-x64.so` | [Download](https://github.com/TheNINJALLO/endstone-worldgen-api/releases/download/v0.4.5-alpha.9/endstone-worldgen-api-v0.4.5-alpha.9-bds-1.26.32-linux-x64.so) |
| **Linux x64** | `1.26.33` | `endstone-worldgen-api-v0.4.5-alpha.9-bds-1.26.33-linux-x64.so` | [Download](https://github.com/TheNINJALLO/endstone-worldgen-api/releases/download/v0.4.5-alpha.9/endstone-worldgen-api-v0.4.5-alpha.9-bds-1.26.33-linux-x64.so) |
| **Python Wheel** | `Universal` | `endstone_worldgen_studio-0.4.5a9-py3-none-any.whl` | [Download](https://github.com/TheNINJALLO/endstone-worldgen-api/releases/download/v0.4.5-alpha.9/endstone_worldgen_studio-0.4.5a9-py3-none-any.whl) |

---

## 🏛️ Architecture Overview

```mermaid
graph TD
    A[World Chunk Interceptor Request] -->|ChunkPos & Seed| B[GenerationContext]
    B -->|Submit Task| C[GenerationScheduler ThreadPool]
    C -->|Worker Thread 1..N| D[Custom Generator / Noise Engine]
    D -->|Populate Array| E[ChunkBuffer 3D Array]
    E -->|BLAKE2b Fingerprint| F[Fingerprint Verification]
    C -->|Neighborhood Lock Manager| G[Cross-Chunk Boundary Sync]
    G -->|Apply to Bedrock Chunk| H[Vanilla World Chunk Injection]
```

---

## ⚡ Quickstart Code Examples

### Python API Example
```python
from endstone_worldgen import (
    GenerationScheduler,
    GenerationContext,
    ChunkPos,
    FlatGenerator,
)

# 1. Initialize multi-worker thread pool
scheduler = GenerationScheduler(workers=4)

# 2. Configure generation context
ctx = GenerationContext(world_seed=12345, dimension="overworld", chunk=ChunkPos(0, 0))

# 3. Generate terrain chunk asynchronously
future = scheduler.generate(ctx, FlatGenerator(surface_y=64, base=1, top=2))
chunk_buffer = future.result()

print(f"Generated Chunk (0,0) Fingerprint: {hex(chunk_buffer.fingerprint())}")
print(f"Surface block at (0,64,0): {chunk_buffer.get(0, 64, 0)}")

scheduler.close()
```

### C++ API Example
```cpp
#include <endstone_worldgen/endstone_adapter.h>
#include <endstone/endstone.hpp>

void generateTerrainAsync(endstone::Server& server) {
    auto* scheduler = server.getServiceManager().getService<endstone_worldgen::GenerationScheduler>();
    if (scheduler) {
        // Submit terrain generation pipeline
    }
}
```

---

## 🎮 In-Game Studio Test Suite (`/wg`)

The repository includes a packaged Python wheel studio plugin [`endstone_worldgen_studio`](examples/python/world_gen_studio_plugin/):

```bash
# Installation via pip in Endstone Python environment:
pip install endstone_worldgen_studio-0.4.5a9-py3-none-any.whl
```

### In-Game Command Reference
| Command | Usage | Description |
| :--- | :--- | :--- |
| `/wg gen <flat\|island\|maze>` | `/wg gen island 0 0` | Submits terrain chunk generation task using specified custom generator. |
| `/wg structure <castle\|arena>` | `/wg structure castle 0 0` | Tests neighborhood locking across a 3x3 chunk boundary grid. |
| `/wg benchmark [chunk_count]` | `/wg benchmark 100` | Runs multi-threaded stress test across N chunks and reports chunks/sec throughput. |
| `/wg inspect [cx] [cz]` | `/wg inspect 0 0` | Displays chunk min/max Y, surface blocks, and BLAKE2b fingerprint hash. |

---

## 📚 Documentation & Wiki

Full technical documentation, architecture deep dives, and API reference manuals are available in the project Wiki:

- 📖 [Documentation Index](docs/README.md)
- 🏗️ [Architecture & Multi-Threading Model](docs/ARCHITECTURE.md)
- ⚙️ [Generators, Contexts & Pipeline Stages](docs/generators_and_stages.md)
- 🔒 [Neighborhood Boundary Lock Engine](docs/neighborhood_locking.md)
- 📘 [Complete API Reference](docs/api_reference.md)
- 💡 [Code Examples & Recipes](examples/python/)

---

## 📜 License

Distributed under the [MIT License](LICENSE).
