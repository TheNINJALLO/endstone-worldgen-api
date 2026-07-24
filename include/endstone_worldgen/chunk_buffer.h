#pragma once
#include "endstone_worldgen/types.h"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace endstone_worldgen {
using DescriptorStateValue = std::variant<bool, std::int32_t, std::string>;
using DescriptorStates = std::unordered_map<std::string, DescriptorStateValue>;

struct BlockDescriptor {
    std::string type{"minecraft:air"};
    DescriptorStates states;
};

class ChunkBuffer {
public:
    ChunkBuffer(ChunkPos pos, std::int32_t min_y=-64, std::int32_t max_y=319, std::uint32_t fill=0);
    [[nodiscard]] ChunkPos position() const noexcept { return pos_; }
    [[nodiscard]] std::int32_t minY() const noexcept { return min_y_; }
    [[nodiscard]] std::int32_t maxY() const noexcept { return max_y_; }
    [[nodiscard]] std::uint32_t getRuntimeId(int x,int y,int z) const;
    void setRuntimeId(int x,int y,int z,std::uint32_t id);
    void fill(int min_x,int min_y,int min_z,int max_x,int max_y,int max_z,std::uint32_t id);
    void replace(std::uint32_t from,std::uint32_t to);
    void setBiome(int x,int y,int z,std::uint32_t biome);
    void setPaletteEntry(std::uint32_t runtime_id, BlockDescriptor descriptor);
    [[nodiscard]] const BlockDescriptor *paletteEntry(std::uint32_t runtime_id) const noexcept;
    [[nodiscard]] std::size_t paletteSize() const noexcept { return palette_.size(); }
    [[nodiscard]] std::uint64_t fingerprint() const;
private:
    size_t index(int x,int y,int z) const;
    ChunkPos pos_; std::int32_t min_y_; std::int32_t max_y_; std::vector<std::uint32_t> blocks_;
    std::unordered_map<std::uint32_t,std::uint32_t> biomes_;
    std::unordered_map<std::uint32_t,BlockDescriptor> palette_;
};
}
