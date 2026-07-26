#include "endstone_worldgen/live_generation.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace endstone_worldgen {
namespace {

enum class Recipe { Flat, Island, Maze, Ores, Castle, Arena };

struct TargetChunk {
    ChunkPos position;
    int offset_x{};
    int offset_z{};
};

struct PlannedChunk {
    ChunkBuffer buffer;
    int offset_x{};
    int offset_z{};
    std::size_t changes{};
    std::optional<int> min_changed_y;
    std::optional<int> max_changed_y;
};

using ResolvedPalette = std::unordered_map<std::string, ResolvedBlockDescriptor>;

std::optional<Recipe> parseRecipe(std::string_view name) noexcept {
    if (name == "flat") return Recipe::Flat;
    if (name == "island") return Recipe::Island;
    if (name == "maze") return Recipe::Maze;
    if (name == "ores") return Recipe::Ores;
    if (name == "castle") return Recipe::Castle;
    if (name == "arena") return Recipe::Arena;
    return std::nullopt;
}

LiveGenerationResult failed(LiveGenerationFailure failure, std::string message) {
    LiveGenerationResult result;
    result.failure = failure;
    result.message = std::move(message);
    return result;
}

bool offsetChunk(ChunkPos center, int offset_x, int offset_z, ChunkPos &out) noexcept {
    const auto x = static_cast<std::int64_t>(center.x) + offset_x;
    const auto z = static_cast<std::int64_t>(center.z) + offset_z;
    if (x < std::numeric_limits<std::int32_t>::min() ||
        x > std::numeric_limits<std::int32_t>::max() ||
        z < std::numeric_limits<std::int32_t>::min() ||
        z > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    out = {static_cast<std::int32_t>(x), static_cast<std::int32_t>(z)};
    return true;
}

std::optional<std::vector<TargetChunk>> targetChunks(ChunkPos center, Recipe recipe) {
    std::vector<TargetChunk> targets;
    if (recipe != Recipe::Castle && recipe != Recipe::Arena) {
        targets.push_back({center, 0, 0});
        return targets;
    }
    targets.reserve(9);
    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
        for (int offset_z = -1; offset_z <= 1; ++offset_z) {
            ChunkPos position;
            if (!offsetChunk(center, offset_x, offset_z, position)) return std::nullopt;
            targets.push_back({position, offset_x, offset_z});
        }
    }
    return targets;
}

std::vector<std::string> requiredBlocks(Recipe recipe) {
    switch (recipe) {
    case Recipe::Flat:
        return {"minecraft:stone", "minecraft:grass_block"};
    case Recipe::Island:
        return {"minecraft:stone", "minecraft:dirt", "minecraft:grass_block"};
    case Recipe::Maze:
        return {"minecraft:stone", "minecraft:cobblestone"};
    case Recipe::Ores:
        return {"minecraft:diamond_ore", "minecraft:deepslate_diamond_ore"};
    case Recipe::Castle:
        return {"minecraft:cobblestone", "minecraft:gold_block"};
    case Recipe::Arena:
        return {"minecraft:stone", "minecraft:glass", "minecraft:diamond_block"};
    }
    return {};
}

std::optional<int> anchorY(const ChunkBuffer &buffer, int requested, int below, int above) noexcept {
    const auto low = static_cast<std::int64_t>(buffer.minY()) + below;
    const auto high = static_cast<std::int64_t>(buffer.maxY()) - above;
    if (low > high || requested < low || requested > high) return std::nullopt;
    return requested;
}

void put(PlannedChunk &plan, int x, int y, int z,
         const ResolvedBlockDescriptor &block) {
    if (x < 0 || x > 15 || z < 0 || z > 15 ||
        y < plan.buffer.minY() || y > plan.buffer.maxY()) {
        return;
    }
    plan.buffer.setPaletteEntry(block.runtime_id, block.descriptor);
    if (plan.buffer.getRuntimeId(x, y, z) == block.runtime_id) return;
    plan.buffer.setRuntimeId(x, y, z, block.runtime_id);
    ++plan.changes;
    if (!plan.min_changed_y || y < *plan.min_changed_y) plan.min_changed_y = y;
    if (!plan.max_changed_y || y > *plan.max_changed_y) plan.max_changed_y = y;
}

void includeChangedYRange(LiveGenerationResult &result, const PlannedChunk &plan) {
    if (!plan.min_changed_y || !plan.max_changed_y) return;
    if (!result.has_changed_y_range) {
        result.has_changed_y_range = true;
        result.min_changed_y = *plan.min_changed_y;
        result.max_changed_y = *plan.max_changed_y;
        return;
    }
    result.min_changed_y = std::min(result.min_changed_y, *plan.min_changed_y);
    result.max_changed_y = std::max(result.max_changed_y, *plan.max_changed_y);
}

const ResolvedBlockDescriptor &block(const ResolvedPalette &palette, const char *name) {
    return palette.at(name);
}

bool applyFlat(PlannedChunk &plan, const ResolvedPalette &palette, int anchor_y) {
    const auto y = anchorY(plan.buffer, anchor_y, 1, 0);
    if (!y) return false;
    const auto &stone = block(palette, "minecraft:stone");
    const auto &grass = block(palette, "minecraft:grass_block");
    for (int x = 3; x <= 12; ++x) {
        for (int z = 3; z <= 12; ++z) {
            put(plan, x, *y - 1, z, stone);
            put(plan, x, *y, z, grass);
        }
    }
    return true;
}

bool applyIsland(PlannedChunk &plan, const ResolvedPalette &palette, int anchor_y) {
    const auto y = anchorY(plan.buffer, anchor_y, 4, 0);
    if (!y) return false;
    const auto &stone = block(palette, "minecraft:stone");
    const auto &dirt = block(palette, "minecraft:dirt");
    const auto &grass = block(palette, "minecraft:grass_block");
    for (int x = 1; x <= 15; ++x) {
        for (int z = 1; z <= 15; ++z) {
            const int dx = x - 8;
            const int dz = z - 8;
            const int radius_squared = dx * dx + dz * dz;
            if (radius_squared > 36) continue;
            const int depth = radius_squared <= 9 ? 3 : (radius_squared <= 25 ? 2 : 1);
            for (int offset = 2; offset <= depth + 1; ++offset) {
                put(plan, x, *y - offset, z, stone);
            }
            put(plan, x, *y - 1, z, dirt);
            put(plan, x, *y, z, grass);
        }
    }
    return true;
}

bool applyMaze(PlannedChunk &plan, const ResolvedPalette &palette, int anchor_y) {
    const auto y = anchorY(plan.buffer, anchor_y, 0, 3);
    if (!y) return false;
    const auto &stone = block(palette, "minecraft:stone");
    const auto &cobble = block(palette, "minecraft:cobblestone");
    for (int x = 1; x <= 14; ++x) {
        for (int z = 1; z <= 14; ++z) {
            put(plan, x, *y, z, stone);
            const bool boundary = x == 1 || x == 14 || z == 1 || z == 14;
            const bool interior_wall = (x % 4 == 0 && z % 4 != 2) ||
                                       (z % 4 == 0 && x % 4 == 2);
            if (!boundary && !interior_wall) continue;
            for (int wall_y = *y + 1; wall_y <= *y + 3; ++wall_y) {
                put(plan, x, wall_y, z, cobble);
            }
        }
    }
    return true;
}

bool applyOres(PlannedChunk &plan, const ResolvedPalette &palette) {
    const auto &diamond = block(palette, "minecraft:diamond_ore");
    const auto &deep_diamond = block(palette, "minecraft:deepslate_diamond_ore");
    const auto low = static_cast<std::int64_t>(plan.buffer.minY()) + 1;
    const auto high = static_cast<std::int64_t>(plan.buffer.maxY()) - 1;
    if (low > high) return false;
    const auto span = static_cast<std::uint64_t>(high - low + 1);
    const auto chunk_mix = static_cast<std::uint64_t>(static_cast<std::uint32_t>(plan.buffer.position().x)) *
                               0x9e3779b1ULL ^
                           static_cast<std::uint64_t>(static_cast<std::uint32_t>(plan.buffer.position().z)) *
                               0x85ebca77ULL;
    for (std::uint64_t index = 0; index < 32; ++index) {
        const int x = static_cast<int>((index * 5 + 3) & 15);
        const int z = static_cast<int>((index * 11 + 7) & 15);
        const int y = static_cast<int>(low + ((index * 17 + chunk_mix) % span));
        const auto current_id = plan.buffer.getRuntimeId(x, y, z);
        const auto *current = plan.buffer.paletteEntry(current_id);
        if (!current) continue;
        if (current->type == "minecraft:stone") put(plan, x, y, z, diamond);
        else if (current->type == "minecraft:deepslate") put(plan, x, y, z, deep_diamond);
    }
    return true;
}

bool applyCastle(PlannedChunk &plan, const ResolvedPalette &palette, int anchor_y) {
    const auto y = anchorY(plan.buffer, anchor_y, 0, 7);
    if (!y) return false;
    const auto &cobble = block(palette, "minecraft:cobblestone");
    const auto &gold = block(palette, "minecraft:gold_block");
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            const int global_x = (plan.offset_x + 1) * 16 + x;
            const int global_z = (plan.offset_z + 1) * 16 + z;
            const bool edge = global_x <= 1 || global_x >= 46 ||
                              global_z <= 1 || global_z >= 46;
            const bool gate = (global_x >= 22 && global_x <= 25 &&
                               (global_z <= 1 || global_z >= 46)) ||
                              (global_z >= 22 && global_z <= 25 &&
                               (global_x <= 1 || global_x >= 46));
            const bool corner_tower =
                (global_x <= 4 || global_x >= 43) &&
                (global_z <= 4 || global_z >= 43);
            if ((global_x % 8 == 0 && global_z % 8 == 0) || edge || corner_tower) {
                put(plan, x, *y, z, cobble);
            }
            if (edge && !gate) {
                for (int wall_y = *y + 1; wall_y <= *y + 4; ++wall_y) {
                    put(plan, x, wall_y, z, cobble);
                }
            }
            if (corner_tower) {
                for (int tower_y = *y + 1; tower_y <= *y + 6; ++tower_y) {
                    put(plan, x, tower_y, z, cobble);
                }
            }
            const bool tower_cap =
                (global_x == 3 || global_x == 44) &&
                (global_z == 3 || global_z == 44);
            if (tower_cap) put(plan, x, *y + 7, z, gold);
        }
    }
    return true;
}

bool applyArena(PlannedChunk &plan, const ResolvedPalette &palette, int anchor_y) {
    const auto y = anchorY(plan.buffer, anchor_y, 0, 4);
    if (!y) return false;
    const auto &stone = block(palette, "minecraft:stone");
    const auto &glass = block(palette, "minecraft:glass");
    const auto &diamond = block(palette, "minecraft:diamond_block");
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            const int global_x = (plan.offset_x + 1) * 16 + x;
            const int global_z = (plan.offset_z + 1) * 16 + z;
            const bool edge = global_x <= 1 || global_x >= 46 ||
                              global_z <= 1 || global_z >= 46;
            const bool gate = (global_x >= 21 && global_x <= 26 &&
                               (global_z <= 1 || global_z >= 46)) ||
                              (global_z >= 21 && global_z <= 26 &&
                               (global_x <= 1 || global_x >= 46));
            const bool floor_marker = (global_x % 8 == 4 && global_z % 8 == 4);
            if (edge || floor_marker) put(plan, x, *y, z, stone);
            if (edge && !gate) {
                for (int wall_y = *y + 1; wall_y <= *y + 4; ++wall_y) {
                    put(plan, x, wall_y, z, glass);
                }
            }
            if (global_x == 24 && global_z == 24) {
                put(plan, x, *y + 1, z, diamond);
            }
        }
    }
    return true;
}

bool applyRecipe(PlannedChunk &plan, Recipe recipe, const ResolvedPalette &palette,
                 int anchor_y) {
    switch (recipe) {
    case Recipe::Flat: return applyFlat(plan, palette, anchor_y);
    case Recipe::Island: return applyIsland(plan, palette, anchor_y);
    case Recipe::Maze: return applyMaze(plan, palette, anchor_y);
    case Recipe::Ores: return applyOres(plan, palette);
    case Recipe::Castle: return applyCastle(plan, palette, anchor_y);
    case Recipe::Arena: return applyArena(plan, palette, anchor_y);
    }
    return false;
}

} // namespace

LiveGenerationResult generateLive(IVanillaGenerationAdapter &adapter,
                                  const std::string &dimension, ChunkPos center,
                                  std::int32_t anchor_y,
                                  std::string_view recipe_name) {
    const auto recipe = parseRecipe(recipe_name);
    if (!recipe) {
        return failed(LiveGenerationFailure::InvalidRecipe,
                      "unknown live recipe: " + std::string(recipe_name));
    }
    if (dimension.empty()) {
        return failed(LiveGenerationFailure::InvalidDimension,
                      "the target dimension name is empty");
    }

    try {
        const auto diagnostics = adapter.diagnostics();
        if (!adapter.verifySymbols() || !diagnostics.exact_build_match) {
            return failed(LiveGenerationFailure::RuntimeGate,
                          "the exact BDS/Endstone runtime gate is not satisfied");
        }
        if (!diagnostics.primary_thread) {
            return failed(LiveGenerationFailure::WrongThread,
                          "live generation must run on the Endstone primary thread");
        }
        const auto capabilities = adapter.capabilities();
        if (!capabilities.capture_chunk || !capabilities.commit_chunk ||
            !capabilities.primary_thread_commit_gate) {
            return failed(LiveGenerationFailure::UnsupportedAdapter,
                          "the exact adapter does not expose the required block-only capture/commit gate");
        }

        const auto targets = targetChunks(center, *recipe);
        if (!targets) {
            return failed(LiveGenerationFailure::InvalidDimension,
                          "the requested structure crosses the supported chunk-coordinate range");
        }

        LiveGenerationResult result;
        result.requested_chunks = targets->size();

        ResolvedPalette palette;
        for (const auto &type : requiredBlocks(*recipe)) {
            auto resolved = adapter.resolveBlock(type);
            if (!resolved) {
                result.failure = LiveGenerationFailure::DescriptorResolution;
                result.message = "the exact server could not resolve BlockData for " + type;
                return result;
            }
            palette.emplace(type, std::move(*resolved));
        }

        std::vector<PlannedChunk> plans;
        plans.reserve(targets->size());
        for (const auto &target : *targets) {
            auto captured = adapter.captureChunk(dimension, target.position);
            if (!captured) {
                result.failure = LiveGenerationFailure::Capture;
                result.message = "capture failed for chunk (" +
                                 std::to_string(target.position.x) + ", " +
                                 std::to_string(target.position.z) + ")";
                return result;
            }
            if (!captured->hasCompletePalette()) {
                result.failure = LiveGenerationFailure::IncompletePalette;
                result.message = "capture returned an incomplete runtime palette for chunk (" +
                                 std::to_string(target.position.x) + ", " +
                                 std::to_string(target.position.z) + ")";
                return result;
            }
            plans.push_back({std::move(*captured), target.offset_x, target.offset_z, 0,
                             std::nullopt, std::nullopt});
        }

        for (auto &plan : plans) {
            if (!applyRecipe(plan, *recipe, palette, anchor_y)) {
                result.failure = LiveGenerationFailure::UnsupportedAdapter;
                result.message = "anchor Y " + std::to_string(anchor_y) +
                                 " does not leave enough vertical clearance for the selected recipe";
                return result;
            }
            result.planned_blocks += plan.changes;
        }

        if (result.planned_blocks == 0) {
            result.success = true;
            result.message = "the live recipe is already present or found no safe source blocks to replace";
            return result;
        }

        for (const auto &plan : plans) {
            if (plan.changes == 0) continue;
            bool commit_ok{};
            try {
                commit_ok = adapter.commitChunk(dimension, plan.buffer);
            } catch (const std::exception &error) {
                result.failure = LiveGenerationFailure::Commit;
                result.message = std::string("commit threw an exception: ") + error.what();
                return result;
            } catch (...) {
                result.failure = LiveGenerationFailure::Commit;
                result.message = "commit threw an unknown exception";
                return result;
            }
            if (!commit_ok) {
                result.failure = LiveGenerationFailure::Commit;
                result.message = "commit failed for chunk (" +
                                 std::to_string(plan.buffer.position().x) + ", " +
                                 std::to_string(plan.buffer.position().z) + ")";
                return result;
            }

            bool flush_ok{};
            try {
                flush_ok = adapter.flushThreadBatch(dimension);
            } catch (const std::exception &error) {
                result.failure = LiveGenerationFailure::Flush;
                result.unconfirmed_blocks = plan.changes;
                includeChangedYRange(result, plan);
                result.message = std::string("flush threw an exception: ") + error.what();
                return result;
            } catch (...) {
                result.failure = LiveGenerationFailure::Flush;
                result.unconfirmed_blocks = plan.changes;
                includeChangedYRange(result, plan);
                result.message = "flush threw an unknown exception";
                return result;
            }
            if (!flush_ok) {
                result.failure = LiveGenerationFailure::Flush;
                result.unconfirmed_blocks = plan.changes;
                includeChangedYRange(result, plan);
                result.message = "commit ran but flush failed for chunk (" +
                                 std::to_string(plan.buffer.position().x) + ", " +
                                 std::to_string(plan.buffer.position().z) + ")";
                return result;
            }
            result.changed_blocks += plan.changes;
            ++result.changed_chunks;
            includeChangedYRange(result, plan);
        }

        result.success = true;
        result.committed = true;
        result.message = "all changed chunks passed commit and flush";
        return result;
    } catch (const std::exception &error) {
        return failed(LiveGenerationFailure::Internal,
                      std::string("live generation failed: ") + error.what());
    } catch (...) {
        return failed(LiveGenerationFailure::Internal,
                      "live generation failed with an unknown exception");
    }
}

} // namespace endstone_worldgen
