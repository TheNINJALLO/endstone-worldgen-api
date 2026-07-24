# Complete API Reference

Complete class, function, and parameter specification for `endstone_worldgen`.

---

## 1. `GenerationScheduler`

### `__init__(workers: int = 4)`
Initializes multi-threaded worker threadpool.

### `generate(context: GenerationContext, generator: Any) -> Future[ChunkBuffer]`
Submits a chunk generation task to the worker pool asynchronously.

### `close()`
Shuts down worker threadpool safely.

---

## 2. `ChunkBuffer`

### `__init__(pos: ChunkPos, min_y: int = -64, max_y: int = 319, fill: int = 0)`
Initializes uncompressed 3D chunk block memory buffer.

### `get(x: int, y: int, z: int) -> int`
Reads block ID at chunk local coordinates `(0<=x<16, min_y<=y<=max_y, 0<=z<16)`.

### `set(x: int, y: int, z: int, value: int)`
Writes block ID at chunk local coordinates.

### `fill(ax, ay, az, bx, by, bz, value: int)`
Fills a 3D bounding box within the chunk with specified block ID.

### `fingerprint() -> int`
Computes 64-bit BLAKE2b hash fingerprint of chunk contents.

---

## 3. `GenerationContext`

- `world_seed: int`
- `dimension: str`
- `chunk: ChunkPos`
- `stage: Stage`
- `stage_seed: int`
