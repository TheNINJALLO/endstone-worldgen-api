#pragma once

#include "endstone_worldgen/native_adapter.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace endstone_worldgen {

enum class LiveGenerationFailure {
    None,
    InvalidRecipe,
    InvalidDimension,
    RuntimeGate,
    WrongThread,
    UnsupportedAdapter,
    DescriptorResolution,
    Capture,
    IncompletePalette,
    Commit,
    Flush,
    Internal,
};

[[nodiscard]] constexpr std::string_view liveGenerationFailureName(
    LiveGenerationFailure failure) noexcept {
    switch (failure) {
    case LiveGenerationFailure::None: return "none";
    case LiveGenerationFailure::InvalidRecipe: return "invalid_recipe";
    case LiveGenerationFailure::InvalidDimension: return "invalid_dimension";
    case LiveGenerationFailure::RuntimeGate: return "runtime_gate";
    case LiveGenerationFailure::WrongThread: return "wrong_thread";
    case LiveGenerationFailure::UnsupportedAdapter: return "unsupported_adapter";
    case LiveGenerationFailure::DescriptorResolution: return "descriptor_resolution";
    case LiveGenerationFailure::Capture: return "capture";
    case LiveGenerationFailure::IncompletePalette: return "incomplete_palette";
    case LiveGenerationFailure::Commit: return "commit";
    case LiveGenerationFailure::Flush: return "flush";
    case LiveGenerationFailure::Internal: return "internal";
    }
    return "internal";
}

struct LiveGenerationResult {
    bool success{};
    // True only when every planned changed chunk passed both commitChunk and
    // flushThreadBatch. A successful zero-change request leaves this false.
    bool committed{};
    LiveGenerationFailure failure{LiveGenerationFailure::None};
    std::size_t requested_chunks{};
    std::size_t planned_blocks{};
    std::size_t changed_blocks{};      // commit + flush confirmed
    std::size_t changed_chunks{};      // commit + flush confirmed
    std::size_t unconfirmed_blocks{};  // commit ran but flush failed
    bool has_changed_y_range{};
    std::int32_t min_changed_y{};
    std::int32_t max_changed_y{};
    std::string message;
};

// Executes a bounded built-in recipe synchronously through the adapter's exact
// primary-thread capture/commit/flush gate. The input chunks are captured first
// and recipes only replace explicitly selected block cells; biomes are untouched.
[[nodiscard]] LiveGenerationResult generateLive(
    IVanillaGenerationAdapter &adapter, const std::string &dimension,
    ChunkPos center, std::int32_t anchor_y, std::string_view recipe_name);

} // namespace endstone_worldgen
