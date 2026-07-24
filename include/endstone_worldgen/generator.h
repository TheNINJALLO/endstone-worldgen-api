#pragma once
#include "endstone_worldgen/chunk_buffer.h"
#include <memory>
#include <string_view>
namespace endstone_worldgen {
class IGenerator { public: virtual ~IGenerator()=default; virtual std::string_view identifier() const noexcept=0; virtual void generate(const GenerationContext&,ChunkBuffer&)=0; };
class IPopulator { public: virtual ~IPopulator()=default; virtual std::string_view identifier() const noexcept=0; virtual int radius() const noexcept=0; virtual void populate(const GenerationContext&,ChunkBuffer&)=0; };
class FlatGenerator final:public IGenerator { public: FlatGenerator(int surface_y,std::uint32_t base,std::uint32_t top):surface_y_(surface_y),base_(base),top_(top){} std::string_view identifier()const noexcept override{return "endstone:flat";} void generate(const GenerationContext&,ChunkBuffer&)override; private:int surface_y_;std::uint32_t base_,top_;};
class HashTerrainGenerator final:public IGenerator { public: HashTerrainGenerator(int sea_level,std::uint32_t stone,std::uint32_t surface):sea_(sea_level),stone_(stone),surface_(surface){} std::string_view identifier()const noexcept override{return "endstone:hash_terrain";} void generate(const GenerationContext&,ChunkBuffer&)override; private:int sea_;std::uint32_t stone_,surface_;};
}
