# In-Game Command & Studio Suite

The repository includes a standalone Python wheel test plugin [`endstone_worldgen_studio`](../examples/python/world_gen_studio_plugin/).

---

## 📦 Installation

```bash
pip install endstone_worldgen_studio-0.4.5a9-py3-none-any.whl
```

---

## 🎮 Command Usage (`/wg`)

All subcommands require OP permission or `wg.admin`.

### 1. `/wg gen <flat|island|maze|ores> [cx] [cz]`
Submits terrain chunk generation task using specified custom generator.
- **Example**: `/wg gen island 0 0`

### 2. `/wg structure <castle|arena> [cx] [cz]`
Tests neighborhood locking across a 3x3 chunk boundary grid around target coordinates.
- **Example**: `/wg structure castle 0 0`

### 3. `/wg benchmark [chunk_count]`
Runs parallel multi-threaded stress test across N chunks and reports throughput (chunks/second), memory usage, and thread safety.
- **Example**: `/wg benchmark 100`

### 4. `/wg inspect [cx] [cz]`
Displays chunk min/max Y, surface blocks, bedrock blocks, and BLAKE2b fingerprint hash.
- **Example**: `/wg inspect 0 0`
