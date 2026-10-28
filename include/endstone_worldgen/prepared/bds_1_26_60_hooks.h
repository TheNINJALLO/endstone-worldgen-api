// Generated offline BDS 1.26.60.6 preparation; never a qualified runtime profile.
#pragma once
#include <cstdint>
#include <optional>
namespace endstone_worldgen::prepared::bds_1_26_60 {
inline constexpr bool RuntimeQualified = false;
inline constexpr bool ReleaseReady = false;
inline constexpr const char* PackageVersion = "1.26.60.6";
namespace Linux {
struct BdsOffsets {
    static constexpr std::optional<std::uintptr_t> get_or_load = std::uintptr_t{0xcfe0bc0ULL};
    static constexpr std::optional<std::uintptr_t> main_chunk_source_vtable_address_point = std::uintptr_t{0xfbcee98ULL};
    static constexpr std::optional<std::uintptr_t> main_create_new = std::uintptr_t{0xcfbbf10ULL};
};
} // namespace Linux
inline constexpr int CreateNewChunkOrdinal = 8;
inline constexpr int GetOrLoadChunkOrdinal = 9;
} // namespace endstone_worldgen::prepared::bds_1_26_60
