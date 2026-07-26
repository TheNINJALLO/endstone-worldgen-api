"""Endstone WorldGen detached-buffer command test plugin."""

from __future__ import annotations

import importlib
import random
import time
from typing import Any

from endstone.command import Command, CommandSender
from endstone.plugin import Plugin

from endstone_worldgen import (
    ChunkBuffer,
    ChunkPos,
    FlatGenerator,
    GenerationContext,
    GenerationScheduler,
    Stage,
)


class CustomIslandGenerator:
    identifier = "endstone:floating_island"

    def __init__(self, core_y: int = 100, radius: int = 7):
        self.core_y = core_y
        self.radius = radius

    def generate(self, context: GenerationContext, buffer: ChunkBuffer) -> None:
        buffer.set_biome(8, self.core_y, 8, 9)
        for x in range(16):
            for z in range(16):
                distance = ((x - 8) ** 2 + (z - 8) ** 2) ** 0.5
                if distance > self.radius:
                    continue
                height = int(self.radius - distance + 3)
                for offset in range(-height, height):
                    y = self.core_y + offset
                    if offset == height - 1:
                        block = 2  # grass
                    elif offset > 0:
                        block = 3  # dirt
                    elif offset == 0:
                        block = 56  # diamond ore core
                    else:
                        block = 1  # stone
                    buffer.set(x, y, z, block)


class CustomMazeGenerator:
    identifier = "endstone:dungeon_maze"

    def generate(self, context: GenerationContext, buffer: ChunkBuffer) -> None:
        del context
        buffer.fill(0, 64, 0, 15, 64, 15, 98)
        buffer.fill(0, 69, 0, 15, 69, 15, 98)
        for x in range(16):
            for z in range(16):
                if x in (0, 15) or z in (0, 15) or (x % 4 == 0 and z % 4 != 2):
                    buffer.fill(x, 65, z, x, 68, z, 98)


class CustomArenaGenerator:
    identifier = "endstone:arena"

    def generate(self, context: GenerationContext, buffer: ChunkBuffer) -> None:
        del context
        buffer.fill(0, 64, 0, 15, 64, 15, 1)
        for y in range(65, 69):
            buffer.fill(0, y, 0, 15, y, 0, 98)
            buffer.fill(0, y, 15, 15, y, 15, 98)
            buffer.fill(0, y, 0, 0, y, 15, 98)
            buffer.fill(15, y, 0, 15, y, 15, 98)
        buffer.set(8, 65, 8, 57)


class CustomOreGenerator:
    identifier = "endstone:ore_test"

    def generate(self, context: GenerationContext, buffer: ChunkBuffer) -> None:
        FlatGenerator(surface_y=64, base=1, top=2).generate(context, buffer)
        rng = random.Random(context.stage_seed)
        for _ in range(32):
            buffer.set(rng.randrange(16), rng.randrange(5, 49), rng.randrange(16), 56)


class WorldGenStudioPlugin(Plugin):
    """Exercise the detached WorldGen scheduler and generators from commands."""

    api_version = "0.11"
    version = "0.4.5-beta.30"
    description = "Interactive in-game WorldGen scheduler and buffer test suite"
    depend = ["worldgen_api"]

    commands = {
        "wg": {
            "description": "WorldGen detached-buffer and scheduler test suite",
            "usages": [
                "/wg",
                (
                    "/wg (gen)<action: WorldGenGenerateAction> "
                    "(flat|island|maze|ores)<generator: WorldGenGenerator>"
                ),
                (
                    "/wg (gen)<action: WorldGenGenerateAtAction> "
                    "(flat|island|maze|ores)<generator: WorldGenGeneratorAt> "
                    "<chunk_x: int> <chunk_z: int>"
                ),
                (
                    "/wg (structure)<action: WorldGenStructureAction> "
                    "(castle|arena)<structure: WorldGenStructure>"
                ),
                (
                    "/wg (structure)<action: WorldGenStructureAtAction> "
                    "(castle|arena)<structure: WorldGenStructureAt> "
                    "<chunk_x: int> <chunk_z: int>"
                ),
                "/wg (benchmark)<action: WorldGenBenchmarkAction>",
                (
                    "/wg (benchmark)<action: WorldGenBenchmarkCountAction> "
                    "<chunk_count: int>"
                ),
                "/wg (inspect)<action: WorldGenInspectAction>",
                (
                    "/wg (inspect)<action: WorldGenInspectAtAction> "
                    "<chunk_x: int> <chunk_z: int>"
                ),
                "/wg (status)<action: WorldGenStatusAction>",
            ],
            "permissions": ["wg.admin"],
        }
    }

    permissions = {
        "wg.admin": {
            "description": "Allows access to WorldGen Studio commands",
            "default": "op",
        }
    }

    _SUBCOMMAND_HANDLERS = {
        "gen": "_handle_gen",
        "structure": "_handle_structure",
        "benchmark": "_handle_benchmark",
        "inspect": "_handle_inspect",
        "status": "_handle_status",
    }

    def on_enable(self) -> None:
        self.scheduler = None
        self.live_bridge = None
        self.bridge_error = "native bridge was not initialized"
        self._connect_bridge()

        if self.live_bridge is None:
            self.logger.error(
                "WorldGen live bridge unavailable; /wg commands will report unavailable: %s",
                self.bridge_error,
            )
        else:
            self.logger.info("WorldGen Studio enabled against the native endstone:worldgen service.")

    def _connect_bridge(self) -> Any | None:
        """Connect to the native service, allowing command-time recovery."""
        try:
            bridge = importlib.import_module("_endstone_worldgen_live")
            if not bridge.available(self.server):
                self.live_bridge = None
                self.bridge_error = "endstone:worldgen native service is not registered"
                return None
            if getattr(self, "scheduler", None) is None:
                self.scheduler = GenerationScheduler(workers=4)
        except Exception as error:
            self.live_bridge = None
            self.bridge_error = str(error)
            return None

        self.live_bridge = bridge
        self.bridge_error = ""
        return bridge

    def on_disable(self) -> None:
        scheduler = getattr(self, "scheduler", None)
        if scheduler is not None:
            scheduler.close()
            self.scheduler = None

    def on_command(
        self, sender: CommandSender, command: Command, args: list[str]
    ) -> bool:
        if command.name != "wg":
            return False

        if not args:
            self._send_help(sender)
            return True

        handler_name = self._SUBCOMMAND_HANDLERS.get(args[0].lower())
        if handler_name is None:
            self._send_help(sender)
            return True
        return getattr(self, handler_name)(sender, args[1:])

    def _send_help(self, sender: CommandSender) -> None:
        sender.send_message("§e=== WorldGen Studio Test Plugin (v0.4.5-beta.30) ===")
        sender.send_message(
            "§a/wg gen <flat|island|maze|ores> [cx cz] §7- Generate a detached buffer"
        )
        sender.send_message(
            "§a/wg structure <castle|arena> [cx cz] §7- Generate a 3x3 structure test"
        )
        sender.send_message(
            "§a/wg benchmark [chunk_count]          §7- Run a scheduler stress test"
        )
        sender.send_message(
            "§a/wg inspect [cx cz]                  §7- Inspect a generated buffer"
        )
        sender.send_message(
            "§a/wg status                           §7- Inspect native interceptor status"
        )

    def _native_status(self, sender: CommandSender) -> dict | None:
        bridge = getattr(self, "live_bridge", None)
        if bridge is None:
            bridge = self._connect_bridge()
        if bridge is None:
            reason = getattr(self, "bridge_error", "native bridge is unavailable")
            sender.send_message(f"§cNative WorldGen service unavailable: {reason}")
            sender.send_message(
                "§7Install the matching exact native bundle and expose its python/ directory."
            )
            return None
        try:
            raw_status = dict(bridge.status(self.server))
        except Exception as error:
            sender.send_message(f"§cNative WorldGen status failed: {error}")
            return None
        status = {
            **raw_status,
            **dict(raw_status.get("stats") or {}),
            **dict(raw_status.get("diagnostics") or {}),
        }
        if not status.get("interception_active", False):
            sender.send_message("§cNative WorldGen service is loaded, but interception is inactive.")
            return None
        return status

    def _announce_reference_test(self, sender: CommandSender) -> dict | None:
        status = self._native_status(sender)
        if status is None:
            return None
        sender.send_message(
            "§7Native service is active. Running a detached Python reference-buffer test; "
            "this command does not commit chunks to the live world."
        )
        return status

    @staticmethod
    def _dimension_name(sender: CommandSender) -> str:
        location = getattr(sender, "location", None)
        dimension = getattr(location, "dimension", None)
        name = getattr(dimension, "name", None)
        return str(name) if name else "overworld"

    def _get_target_chunk(
        self, sender: CommandSender, args: list[str]
    ) -> tuple[str, int, int]:
        location = getattr(sender, "location", None)
        if location is None:
            chunk_x, chunk_z = 0, 0
        else:
            chunk_x, chunk_z = int(location.x) >> 4, int(location.z) >> 4

        if len(args) >= 2:
            try:
                chunk_x, chunk_z = int(args[0]), int(args[1])
            except ValueError:
                pass
        return self._dimension_name(sender), chunk_x, chunk_z

    def _handle_gen(self, sender: CommandSender, args: list[str]) -> bool:
        if not args or args[0].lower() not in {"flat", "island", "maze", "ores"}:
            sender.send_message("§cUsage: /wg gen <flat|island|maze|ores> [cx] [cz]")
            return True
        if self._announce_reference_test(sender) is None:
            return True

        generator_name = args[0].lower()
        dimension, chunk_x, chunk_z = self._get_target_chunk(sender, args[1:])
        generators = {
            "flat": lambda: FlatGenerator(surface_y=64, base=1, top=2),
            "island": CustomIslandGenerator,
            "maze": CustomMazeGenerator,
            "ores": CustomOreGenerator,
        }
        generator = generators[generator_name]()
        context = GenerationContext(99999, dimension, ChunkPos(chunk_x, chunk_z))

        sender.send_message(
            f"§eGenerating detached buffer ({chunk_x}, {chunk_z}) with '{generator_name}'..."
        )
        started = time.perf_counter()
        buffer = self.scheduler.generate(context, generator).result()
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        sender.send_message(
            f"§aGenerated ({chunk_x}, {chunk_z}) in {elapsed_ms:.2f}ms. "
            f"Fingerprint: {hex(buffer.fingerprint())}"
        )
        return True

    def _handle_structure(self, sender: CommandSender, args: list[str]) -> bool:
        if not args or args[0].lower() not in {"castle", "arena"}:
            sender.send_message("§cUsage: /wg structure <castle|arena> [cx] [cz]")
            return True
        if self._announce_reference_test(sender) is None:
            return True

        structure_name = args[0].lower()
        dimension, center_x, center_z = self._get_target_chunk(sender, args[1:])
        generator_type = CustomMazeGenerator if structure_name == "castle" else CustomArenaGenerator
        sender.send_message(
            f"§eGenerating 3x3 '{structure_name}' buffer set around ({center_x}, {center_z})..."
        )

        futures = []
        for offset_x in range(-1, 2):
            for offset_z in range(-1, 2):
                chunk_x, chunk_z = center_x + offset_x, center_z + offset_z
                context = GenerationContext(
                    88888,
                    dimension,
                    ChunkPos(chunk_x, chunk_z),
                    stage=Stage.STRUCTURES,
                )
                futures.append(self.scheduler.generate(context, generator_type()))

        completed = sum(future.result() is not None for future in futures)
        sender.send_message(f"§aGenerated {completed}/9 structure buffers successfully.")
        return True

    def _handle_benchmark(self, sender: CommandSender, args: list[str]) -> bool:
        try:
            count = int(args[0]) if args else 100
        except ValueError:
            sender.send_message("§cChunk count must be an integer from 1 to 128.")
            return True
        if self._announce_reference_test(sender) is None:
            return True
        count = max(1, min(count, 128))

        sender.send_message(f"§eGenerating {count} buffers with 4 worker threads...")
        started = time.perf_counter()
        generator = FlatGenerator(surface_y=64, base=1, top=2)
        futures = []
        for index in range(count):
            context = GenerationContext(
                77777,
                "overworld",
                ChunkPos(index % 16, index // 16),
            )
            futures.append(self.scheduler.generate(context, generator))
        results = [future.result() for future in futures]
        elapsed = max(time.perf_counter() - started, 1e-9)

        sender.send_message("§a=== WorldGen Benchmark Results ===")
        sender.send_message(f"  §7- Buffers Generated: §f{len(results)}")
        sender.send_message(f"  §7- Elapsed Time: §f{elapsed:.3f} seconds")
        sender.send_message(f"  §7- Throughput: §b{count / elapsed:.1f} buffers/sec")
        return True

    def _handle_inspect(self, sender: CommandSender, args: list[str]) -> bool:
        status = self._announce_reference_test(sender)
        if status is None:
            return True
        dimension, chunk_x, chunk_z = self._get_target_chunk(sender, args)
        context = GenerationContext(12345, dimension, ChunkPos(chunk_x, chunk_z))
        buffer = self.scheduler.generate(
            context, FlatGenerator(surface_y=64, base=1, top=2)
        ).result()

        sender.send_message(f"§e=== Inspecting Chunk Buffer ({chunk_x}, {chunk_z}) ===")
        sender.send_message(f"  §7- Min Y: §f{buffer.min_y} §7Max Y: §f{buffer.max_y}")
        sender.send_message(f"  §7- Surface Y=64 Block: §b{buffer.get(0, 64, 0)}")
        sender.send_message(
            f"  §7- Bottom Y={buffer.min_y} Block: §b{buffer.get(0, buffer.min_y, 0)}"
        )
        sender.send_message(f"  §7- FNV-1a Fingerprint: §f{hex(buffer.fingerprint())}")
        sender.send_message(
            f"  §7- Native requests: §f{status.get('intercepted_requests', 0)} "
            f"§7committed: §f{status.get('committed', 0)}"
        )
        return True

    def _handle_status(self, sender: CommandSender, args: list[str]) -> bool:
        del args
        status = self._native_status(sender)
        if status is None:
            return True
        sender.send_message("§e=== Native WorldGen Service Status ===")
        sender.send_message(
            f"§7Interception Active: §a{status.get('interception_active', False)} "
            f"§7Populators: §f{status.get('populator_count', 0)}"
        )
        sender.send_message(
            f"§7Intercepted: §f{status.get('intercepted_requests', 0)} "
            f"§7Dispatched: §f{status.get('dispatched', 0)} "
            f"§7Committed: §f{status.get('committed', 0)}"
        )
        sender.send_message(
            f"§7Waiting: §f{status.get('waiting', 0)} "
            f"§7In-flight: §f{status.get('inflight', 0)} "
            f"§7Native dropped: §f{status.get('dropped_requests', 0)} "
            f"§7Waiting dropped: §f{status.get('waiting_overflow_drops', 0)}"
        )
        return True
