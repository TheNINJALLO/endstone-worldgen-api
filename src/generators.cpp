#include "endstone_worldgen/generator.h"
namespace endstone_worldgen {

std::uint64_t deterministicStringHash(std::string_view value) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::uint64_t deterministicStageSeed(std::uint64_t seed, const std::string &dimension,
                                     ChunkPos chunk, GenerationStage stage) {
    std::uint64_t hash = seed;
    const auto mix = [&hash](std::uint64_t value) {
        hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    };
    mix(deterministicStringHash(dimension));
    mix(static_cast<std::uint32_t>(chunk.x));
    mix(static_cast<std::uint32_t>(chunk.z));
    mix(static_cast<std::uint32_t>(stage));
    return hash;
}

void FlatGenerator::generate(const GenerationContext &, ChunkBuffer &chunk) {
    if (surface_y_ > chunk.minY()) {
        chunk.fill(0, chunk.minY(), 0, 15, surface_y_ - 1, 15, base_);
    }
    if (surface_y_ >= chunk.minY() && surface_y_ <= chunk.maxY()) {
        chunk.fill(0, surface_y_, 0, 15, surface_y_, 15, top_);
    }
}

void HashTerrainGenerator::generate(const GenerationContext &context, ChunkBuffer &chunk) {
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            const auto world_x = static_cast<std::int64_t>(context.chunk.x) * 16 + x;
            const auto world_z = static_cast<std::int64_t>(context.chunk.z) * 16 + z;
            std::uint64_t hash = context.stage_seed ^
                                 (static_cast<std::uint64_t>(world_x) * 341873128712ULL) ^
                                 (static_cast<std::uint64_t>(world_z) * 132897987541ULL);
            hash ^= hash >> 33;
            hash *= 0xff51afd7ed558ccdULL;
            const auto height = static_cast<std::int64_t>(sea_) +
                                static_cast<std::int64_t>(hash % 25) - 12;
            if (height > chunk.minY()) {
                const auto stone_top = std::min<std::int64_t>(height - 1, chunk.maxY());
                chunk.fill(x, chunk.minY(), z, x, static_cast<int>(stone_top), z, stone_);
            }
            if (height >= chunk.minY() && height <= chunk.maxY()) {
                chunk.setRuntimeId(x, static_cast<int>(height), z, surface_);
            }
        }
    }
}
}
