# Generators & Pipeline Stages

The **Endstone WorldGen API** structures terrain generation into discrete, deterministic pipeline stages.

---

## ⚙️ Generation Pipeline Stages

Terrain generation executes through the `Stage` enum in sequential order:

1. `Stage.BIOMES`: Biome map assignment across chunk coordinates.
2. `Stage.BASE_TERRAIN`: Core block noise generation (stone, deepslate, water level).
3. `Stage.SURFACE`: Surface layers (grass, dirt, sand, terracotta).
4. `Stage.CARVERS`: Caves, ravines, and underground tunnels.
5. `Stage.STRUCTURES`: Multi-chunk structures (dungeons, strongholds, custom arenas).
6. `Stage.FEATURES`: Ore veins, trees, flora, and scatter features.
7. `Stage.DECORATION`: Vegetation, snow layers, and fine details.
8. `Stage.FINALIZATION`: Bedrock chunk injection and lighting.

---

## 🧱 Implementing a Custom Generator

A custom generator requires an `identifier` string and a `generate(ctx, buf)` method:

```python
from endstone_worldgen import GenerationContext, ChunkBuffer

class CustomFloatingIslandGenerator:
    identifier = "endstone:floating_island"

    def __init__(self, core_y=100, radius=7):
        self.core_y = core_y
        self.radius = radius

    def generate(self, ctx: GenerationContext, buf: ChunkBuffer):
        buf.biomes[ctx.chunk] = "end_highlands"
        for x in range(16):
            for z in range(16):
                dist = ((x - 8) ** 2 + (z - 8) ** 2) ** 0.5
                if dist <= self.radius:
                    height = int(self.radius - dist + 3)
                    for dy in range(-height, height):
                        y = self.core_y + dy
                        if dy == height - 1:
                            buf.set(x, y, z, 2)  # Grass
                        elif dy > 0:
                            buf.set(x, y, z, 3)  # Dirt
                        elif dy == 0:
                            buf.set(x, y, z, 56) # Diamond Ore Core
                        else:
                            buf.set(x, y, z, 1)  # Stone
```
