import sys
import unittest

sys.path.insert(0, "python")

from endstone_worldgen import (
    BlockDescriptor,
    ChunkBuffer,
    ChunkPos,
    FlatGenerator,
    GenerationContext,
    GenerationScheduler,
    Stage,
    stage_seed,
)


class TestWorldGen(unittest.TestCase):
    def test_deterministic(self):
        scheduler = GenerationScheduler(2)
        self.addCleanup(scheduler.close)
        first_context = GenerationContext(99, "overworld", ChunkPos(2, 3))
        second_context = GenerationContext(99, "overworld", ChunkPos(2, 3))
        generator = FlatGenerator()
        first = scheduler.generate(first_context, generator).result()
        second = scheduler.generate(second_context, generator).result()
        self.assertEqual(first.fingerprint(), second.fingerprint())
        self.assertEqual(first.get(0, 64, 0), 2)
        self.assertEqual(first_context.stage_seed, 0)
        self.assertEqual(second_context.stage_seed, 0)

    def test_stage_seed_matches_native_contract(self):
        context = GenerationContext(
            1234,
            "overworld",
            ChunkPos(2, 3),
            Stage.BASE_TERRAIN,
        )
        self.assertEqual(stage_seed(context), 0xBC6A9B6F10AA3EF4)

    def test_fingerprint_matches_native_contract(self):
        plain = ChunkBuffer(ChunkPos(2, -3), min_y=0, max_y=0, fill=7)
        self.assertEqual(plain.fingerprint(), 0xC8B312F0A708B926)

        plain.set_biome(0, 0, 0, 42)
        plain.set_palette_entry(
            7,
            BlockDescriptor(
                "minecraft:stone",
                {"age": 3, "label": "x", "powered": True},
            ),
        )
        self.assertEqual(plain.fingerprint(), 0xB9F0CA7D18902B33)

    def test_fingerprint_is_order_independent_but_content_sensitive(self):
        first = ChunkBuffer(ChunkPos(1, 2), min_y=0, max_y=0, fill=7)
        second = ChunkBuffer(ChunkPos(1, 2), min_y=0, max_y=0, fill=7)
        first.biomes.update({3: 20, 1: 10})
        second.biomes.update({1: 10, 3: 20})
        first.set_palette_entry(
            7, BlockDescriptor("minecraft:stone", {"powered": True, "age": 3})
        )
        second.set_palette_entry(
            7, BlockDescriptor("minecraft:stone", {"age": 3, "powered": True})
        )
        self.assertEqual(first.fingerprint(), second.fingerprint())

        different_position = ChunkBuffer(ChunkPos(2, 2), min_y=0, max_y=0, fill=7)
        different_position.biomes.update(second.biomes)
        different_position.palette.update(second.palette)
        self.assertNotEqual(first.fingerprint(), different_position.fingerprint())

        second.biomes[1] = 11
        self.assertNotEqual(first.fingerprint(), second.fingerprint())


if __name__ == "__main__":
    unittest.main()
