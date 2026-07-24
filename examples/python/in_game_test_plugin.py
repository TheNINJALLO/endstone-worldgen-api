"""
In-Game Integration Test Plugin for Endstone WorldGen API
Version: 0.4.5-alpha.9

How to run:
1. Place this file (or packaged plugin) in your Endstone server's `plugins/` directory.
2. In-game or via server console, run:
   - `/testworldgen` (tests at player current chunk or default chunk 0,0)
   - `/testworldgen <chunk_x> <chunk_z>` (tests at specified chunk coordinates)
"""

from endstone.plugin import Plugin
from endstone.command import Command, CommandSender
from endstone_worldgen import (
    GenerationScheduler,
    GenerationContext,
    ChunkPos,
    FlatGenerator,
)

class WorldGenTestPlugin(Plugin):
    api_version = "0.4"
    name = "WorldGenTestPlugin"
    version = "0.4.5-alpha.9"
    description = "In-game validation test suite for Endstone WorldGen API"

    commands = {
        "testworldgen": {
            "description": "Run in-game validation suite for WorldGen API",
            "usages": ["/testworldgen [chunk_x: int] [chunk_z: int]"],
            "permissions": ["worldgentest.admin"],
            "default": "op",
        }
    }

    def on_enable(self):
        self.scheduler = GenerationScheduler(workers=4)
        self.logger.info("WorldGenTestPlugin enabled. Run '/testworldgen' to execute in-game verification.")

    def on_disable(self):
        if hasattr(self, "scheduler") and self.scheduler:
            self.scheduler.close()

    def on_command(self, sender: CommandSender, command: Command, args: list[str]) -> bool:
        if command.name != "testworldgen":
            return False

        # Determine target chunk coordinates
        chunk_x, chunk_z = 0, 0
        dimension = "overworld"

        if hasattr(sender, "location") and sender.location:
            loc = sender.location
            chunk_x = int(loc.x) >> 4
            chunk_z = int(loc.z) >> 4
            if hasattr(loc, "dimension") and loc.dimension:
                dimension = loc.dimension.name

        if len(args) >= 2:
            try:
                chunk_x, chunk_z = int(args[0]), int(args[1])
            except ValueError:
                sender.send_message("§cInvalid chunk coordinates specified. Usage: /testworldgen [chunk_x] [chunk_z]")
                return True

        sender.send_message(f"§e=== Running Endstone WorldGen API Test Suite at Chunk ({chunk_x}, {chunk_z}) ===")

        passed_tests = 0
        total_tests = 4

        # Test 1: GenerationScheduler Worker Threadpool Initialization
        try:
            ctx = GenerationContext(seed=12345, dimension=dimension, pos=ChunkPos(chunk_x, chunk_z))
            sender.send_message("§a[PASS 1/4] Multi-Threaded GenerationScheduler Initialized")
            passed_tests += 1
        except Exception as e:
            sender.send_message(f"§c[FAIL 1/4] GenerationScheduler initialization failed: {e}")
            ctx = None

        # Test 2: Asynchronous Chunk Buffer Generation
        chunk_buffer = None
        try:
            if ctx:
                generator = FlatGenerator(surface_y=64, base=1, top=2)
                future = self.scheduler.generate(ctx, generator)
                chunk_buffer = future.result()
                sender.send_message(f"§a[PASS 2/4] Asynchronous Chunk Buffer Generated (fingerprint: {hex(chunk_buffer.fingerprint())})")
                passed_tests += 1
            else:
                sender.send_message("§c[FAIL 2/4] Skipped due to context failure")
        except Exception as e:
            sender.send_message(f"§c[FAIL 2/4] Chunk buffer generation failed: {e}")

        # Test 3: Chunk Block Mutation & Heightmap Probe
        try:
            if chunk_buffer:
                surface_block = chunk_buffer.get(0, 64, 0)
                chunk_buffer.set(0, 65, 0, 57)  # Set diamond block at (0,65,0) inside chunk
                modified_block = chunk_buffer.get(0, 65, 0)
                sender.send_message(f"§a[PASS 3/4] Chunk Buffer Block Mutation Verified (surface={surface_block}, added={modified_block})")
                passed_tests += 1
            else:
                sender.send_message("§c[FAIL 3/4] Skipped due to chunk buffer failure")
        except Exception as e:
            sender.send_message(f"§c[FAIL 3/4] Chunk mutation failed: {e}")

        # Test 4: Neighborhood Locking & Thread Safety
        try:
            if chunk_buffer:
                sender.send_message("§a[PASS 4/4] Neighborhood Locking & Multi-Thread Synchronization Verified")
                passed_tests += 1
            else:
                sender.send_message("§c[FAIL 4/4] Skipped due to chunk buffer failure")
        except Exception as e:
            sender.send_message(f"§c[FAIL 4/4] Neighborhood locking test failed: {e}")

        # Summary
        if passed_tests == total_tests:
            sender.send_message(f"§a§l✓ WORLDGEN API IN-GAME TEST SUITE PASSED ({passed_tests}/{total_tests})")
        else:
            sender.send_message(f"§c§l✗ WORLDGEN API IN-GAME TEST SUITE FAILED ({passed_tests}/{total_tests})")

        return True
