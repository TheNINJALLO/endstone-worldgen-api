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

### `set_biome(x: int, y: int, z: int, runtime_id: int)`
Sets the numeric biome runtime ID for the matching 4x4x4 biome cell in the
detached buffer. Endstone 0.11 exposes no verified live biome-write surface, so
the exact native adapter rejects buffers containing biome edits before changing
any live blocks.

### `set_palette_entry(runtime_id: int, descriptor: BlockDescriptor)`
Associates a runtime ID with its namespaced block type and typed state map.
Every runtime ID used by a buffer must have a descriptor before native commit.
The adapter resolves and verifies the complete used palette before its first
live block mutation.

### `palette_entry(runtime_id: int) -> BlockDescriptor | None`
Returns a detached copy of the matching palette descriptor.

### `fingerprint() -> int`
Computes the canonical 64-bit FNV-1a fingerprint over chunk position, vertical
range, runtime IDs, sorted biome cells, and sorted palette descriptors. The
Python and native implementations use the same byte-level contract.

---

## 3. `GenerationContext`

- `world_seed: int`
- `dimension: str`
- `chunk: ChunkPos`
- `stage: Stage`
- `stage_seed: int`

`stage_seed(context)` uses the same unsigned 64-bit FNV-1a and mixing contract
as native `deterministicStageSeed`, including native stage ordinals and signed
chunk coordinates converted to `uint32`.
