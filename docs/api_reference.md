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

---

## 4. Native `WorldGenService` live recipe path

### `generateLive(dimension, center, anchor_y, recipe) -> LiveGenerationResult`

Runs one of `flat`, `island`, `maze`, `ores`, `castle`, or `arena` through the
exact adapter on the Endstone primary thread. `castle` and `arena` target a 3x3
chunk area; other recipes target one chunk. All target chunks and complete
runtime palettes are captured before the first write. Recipe descriptors are
resolved through the running server, only explicitly selected cells change, and
biomes are never edited.

`anchor_y` is the visible surface/floor for `flat`, `island`, `maze`, `castle`,
and `arena`. The command wheel supplies `floor(sender.location.y) - 1`. Their
bounded write bands are respectively `anchor_y-1..anchor_y`,
`anchor_y-4..anchor_y`, `anchor_y..anchor_y+3`,
`anchor_y..anchor_y+7`, and `anchor_y..anchor_y+4`. Every captured chunk must
contain the full band or the request fails before commit. `ores` does not use
the anchor: it scans the captured vertical range and replaces only natural
stone/deepslate, so its changes may be underground.

`success` means the request completed. `committed` is true only when every
changed chunk passed both `commitChunk` and `flushThreadBatch`. A successful
zero-change request has `committed == false`. `changed_blocks` and
`changed_chunks` count flush-confirmed changes; `unconfirmed_blocks` reports a
commit whose subsequent flush failed. `failure` and `message` provide the
machine-readable and human-readable failure reason. Nullable `min_changed_y`
and `max_changed_y` bound all flush-confirmed changes plus any commit whose flush
could not be confirmed. Both are null when no native write range exists.

### `liveStats() -> LiveGenerationStats`

Returns persistent manual-request, success, no-change, descriptor, capture,
commit, flush, primary-thread rejection, confirmed-block, and unconfirmed-block
counters. These are separate from automatic `GenerationInterceptor` statistics.
