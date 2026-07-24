# Neighborhood Boundary Lock Engine

Generating large structures (e.g. castles, strongholds, custom dungeons) spanning multiple chunk boundaries requires synchronization to prevent race conditions or missing chunk edges.

---

## 🔒 3x3 Neighborhood Locking Concept

When generating a structure centered at chunk `(C_x, C_z)`, the generator requires access to the 8 adjacent surrounding chunks:

```text
[ (cx-1, cz-1) ]  [ (cx, cz-1) ]  [ (cx+1, cz-1) ]
[ (cx-1, cz)   ]  [ (cx, cz)   ]  [ (cx+1, cz)   ]
[ (cx-1, cz+1) ]  [ (cx, cz+1) ]  [ (cx+1, cz+1) ]
```

### Multi-Chunk Generation Example

```python
from endstone_worldgen import GenerationScheduler, GenerationContext, ChunkPos, Stage

scheduler = GenerationScheduler(workers=4)
center_cx, center_cz = 0, 0

futures = []
for dx in range(-1, 2):
    for dz in range(-1, 2):
        cx, cz = center_cx + dx, center_cz + dz
        ctx = GenerationContext(
            world_seed=88888,
            dimension="overworld",
            chunk=ChunkPos(cx, cz),
            stage=Stage.STRUCTURES
        )
        futures.append((cx, cz, scheduler.generate(ctx, custom_generator)))

# Synchronize all 9 surrounding chunk tasks
completed_buffers = [fut.result() for cx, cz, fut in futures]
print("3x3 Neighborhood Generation Complete:", len(completed_buffers) == 9)
```
