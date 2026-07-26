from __future__ import annotations

from concurrent.futures import Future, ThreadPoolExecutor
from dataclasses import dataclass, field
from enum import Enum, IntEnum
from typing import Any


@dataclass(frozen=True, slots=True)
class ChunkPos:
    x: int
    z: int


class Stage(Enum):
    # Keep these values identical to native GenerationStage's declaration order.
    BIOMES = 0
    BASE_TERRAIN = 1
    SURFACE = 2
    CARVERS = 3
    STRUCTURES = 4
    FEATURES = 5
    DECORATION = 6
    BLOCK_ENTITIES = 7
    FINALIZATION = 8


class Priority(IntEnum):
    BACKGROUND = 10
    FORCED = 30
    VIEW_DISTANCE = 60
    TELEPORT = 80
    PLAYER_REQUESTED = 100


@dataclass(slots=True)
class GenerationContext:
    world_seed: int
    dimension: str
    chunk: ChunkPos
    stage: Stage = Stage.BASE_TERRAIN
    stage_seed: int = 0


def stage_seed(context: GenerationContext) -> int:
    """Return the exact 64-bit seed produced by native deterministicStageSeed."""

    mask = (1 << 64) - 1
    value = 14695981039346656037
    for byte in context.dimension.encode("utf-8"):
        value ^= byte
        value = (value * 1099511628211) & mask

    result = context.world_seed & mask
    for component in (
        value,
        context.chunk.x & 0xFFFFFFFF,
        context.chunk.z & 0xFFFFFFFF,
        context.stage.value,
    ):
        combined = (
            component
            + 0x9E3779B97F4A7C15
            + ((result << 6) & mask)
            + (result >> 2)
        ) & mask
        result = (result ^ combined) & mask
    return result


DescriptorStateValue = bool | int | str


@dataclass(slots=True)
class BlockDescriptor:
    type: str = "minecraft:air"
    states: dict[str, DescriptorStateValue] = field(default_factory=dict)


class ChunkBuffer:
    """Detached chunk storage with fingerprint parity with the native buffer."""

    _FNV_OFFSET = 14695981039346656037
    _FNV_PRIME = 1099511628211
    _U64_MASK = (1 << 64) - 1

    def __init__(
        self,
        pos: ChunkPos,
        min_y: int = -64,
        max_y: int = 319,
        fill: int = 0,
    ):
        if min_y > max_y:
            raise ValueError("min_y cannot exceed max_y")
        self.pos = pos
        self.min_y = min_y
        self.max_y = max_y
        self._b = [fill] * (16 * 16 * (max_y - min_y + 1))
        self.biomes: dict[int, int] = {}
        self.palette: dict[int, BlockDescriptor] = {}

    def _i(self, x: int, y: int, z: int) -> int:
        if not (0 <= x < 16 and 0 <= z < 16 and self.min_y <= y <= self.max_y):
            raise IndexError((x, y, z))
        return (y - self.min_y) * 256 + z * 16 + x

    def get(self, x: int, y: int, z: int) -> int:
        return self._b[self._i(x, y, z)]

    def set(self, x: int, y: int, z: int, value: int) -> None:
        self._b[self._i(x, y, z)] = value

    def fill(
        self,
        ax: int,
        ay: int,
        az: int,
        bx: int,
        by: int,
        bz: int,
        value: int,
    ) -> None:
        for x in range(max(0, min(ax, bx)), min(15, max(ax, bx)) + 1):
            for y in range(
                max(self.min_y, min(ay, by)), min(self.max_y, max(ay, by)) + 1
            ):
                for z in range(max(0, min(az, bz)), min(15, max(az, bz)) + 1):
                    self.set(x, y, z, value)

    def set_biome(self, x: int, y: int, z: int, runtime_id: int) -> None:
        self._i(x, y, z)
        key = ((y - self.min_y) // 4) * 16 + (z // 4) * 4 + x // 4
        self.biomes[key] = runtime_id

    def set_palette_entry(
        self, runtime_id: int, descriptor: BlockDescriptor
    ) -> None:
        if not isinstance(descriptor, BlockDescriptor):
            raise TypeError("descriptor must be a BlockDescriptor")
        self.palette[runtime_id] = BlockDescriptor(
            descriptor.type, dict(descriptor.states)
        )

    def palette_entry(self, runtime_id: int) -> BlockDescriptor | None:
        descriptor = self.palette.get(runtime_id)
        if descriptor is None:
            return None
        return BlockDescriptor(descriptor.type, dict(descriptor.states))

    @classmethod
    def _feed(cls, value: int, payload: bytes) -> int:
        for byte in payload:
            value ^= byte
            value = (value * cls._FNV_PRIME) & cls._U64_MASK
        return value

    @classmethod
    def _feed_u32(cls, value: int, integer: int) -> int:
        return cls._feed(value, (integer & 0xFFFFFFFF).to_bytes(4, "little"))

    @classmethod
    def _feed_u64(cls, value: int, integer: int) -> int:
        return cls._feed(value, (integer & cls._U64_MASK).to_bytes(8, "little"))

    @classmethod
    def _feed_string(cls, value: int, string: str) -> int:
        encoded = string.encode("utf-8")
        value = cls._feed_u64(value, len(encoded))
        return cls._feed(value, encoded)

    def fingerprint(self) -> int:
        value = self._FNV_OFFSET
        value = self._feed_u32(value, self.pos.x)
        value = self._feed_u32(value, self.pos.z)
        value = self._feed_u32(value, self.min_y)
        value = self._feed_u32(value, self.max_y)
        value = self._feed_u64(value, len(self._b))
        for runtime_id in self._b:
            value = self._feed_u32(value, runtime_id)

        value = self._feed_u64(value, len(self.biomes))
        for key, runtime_id in sorted(self.biomes.items()):
            value = self._feed_u32(value, key)
            value = self._feed_u32(value, runtime_id)

        value = self._feed_u64(value, len(self.palette))
        for runtime_id, descriptor in sorted(self.palette.items()):
            value = self._feed_u32(value, runtime_id)
            value = self._feed_string(value, descriptor.type)
            value = self._feed_u64(value, len(descriptor.states))
            for key, state_value in sorted(
                descriptor.states.items(), key=lambda item: item[0].encode("utf-8")
            ):
                value = self._feed_string(value, key)
                if isinstance(state_value, bool):
                    value = self._feed(value, b"\x00")
                    value = self._feed(value, bytes((int(state_value),)))
                elif isinstance(state_value, int):
                    value = self._feed(value, b"\x01")
                    value = self._feed_u32(value, state_value)
                elif isinstance(state_value, str):
                    value = self._feed(value, b"\x02")
                    value = self._feed_string(value, state_value)
                else:
                    raise TypeError(
                        f"unsupported state type for {key!r}: {type(state_value).__name__}"
                    )
        return value


class FlatGenerator:
    identifier = "endstone:flat"

    def __init__(self, surface_y: int = 64, base: int = 1, top: int = 2):
        self.surface_y = surface_y
        self.base = base
        self.top = top

    def generate(self, context: GenerationContext, buffer: ChunkBuffer) -> None:
        del context
        buffer.fill(
            0, buffer.min_y, 0, 15, self.surface_y - 1, 15, self.base
        )
        buffer.fill(0, self.surface_y, 0, 15, self.surface_y, 15, self.top)


class GenerationScheduler:
    def __init__(self, workers: int = 4):
        self.pool = ThreadPoolExecutor(
            max_workers=max(1, workers), thread_name_prefix="worldgen"
        )

    def generate(self, context: GenerationContext, generator: Any) -> Future:
        submitted_context = GenerationContext(
            context.world_seed,
            context.dimension,
            context.chunk,
            context.stage,
            context.stage_seed,
        )

        def run() -> ChunkBuffer:
            submitted_context.stage_seed = stage_seed(submitted_context)
            buffer = ChunkBuffer(submitted_context.chunk)
            generator.generate(submitted_context, buffer)
            return buffer

        return self.pool.submit(run)

    def close(self) -> None:
        self.pool.shutdown(wait=True, cancel_futures=True)
