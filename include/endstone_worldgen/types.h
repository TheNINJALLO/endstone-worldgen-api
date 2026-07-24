#pragma once
#include <cstdint>
#include <string>
namespace endstone_worldgen {
struct ChunkPos { std::int32_t x{}; std::int32_t z{}; auto operator<=>(const ChunkPos&) const = default; };
struct BlockPos { std::int32_t x{}; std::int32_t y{}; std::int32_t z{}; };
enum class GenerationStage { Biomes, BaseTerrain, Surface, Carvers, Structures, Features, Decoration, BlockEntities, Finalization };
enum class JobPriority : int { Background=10, Forced=30, ViewDistance=60, Teleport=80, PlayerRequested=100 };
enum class VanillaAccelerationMode { Disabled, PostProcessing, Hybrid, NativeExperimental };
struct GenerationContext { std::uint64_t world_seed{}; std::string dimension{"overworld"}; ChunkPos chunk; GenerationStage stage{GenerationStage::BaseTerrain}; std::uint64_t stage_seed{}; };
std::uint64_t deterministicStageSeed(std::uint64_t world_seed, const std::string &dimension, ChunkPos chunk, GenerationStage stage);
}
