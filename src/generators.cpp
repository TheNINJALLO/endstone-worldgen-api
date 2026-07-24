#include "endstone_worldgen/generator.h"
#include <functional>
namespace endstone_worldgen {
std::uint64_t deterministicStageSeed(std::uint64_t seed,const std::string&d,ChunkPos c,GenerationStage s){std::uint64_t h=seed;auto mix=[&](std::uint64_t v){h^=v+0x9e3779b97f4a7c15ULL+(h<<6)+(h>>2);};mix(std::hash<std::string>{}(d));mix(std::uint32_t(c.x));mix(std::uint32_t(c.z));mix(std::uint32_t(s));return h;}
void FlatGenerator::generate(const GenerationContext&,ChunkBuffer&c){c.fill(0,c.minY(),0,15,surface_y_-1,15,base_);c.fill(0,surface_y_,0,15,surface_y_,15,top_);}
void HashTerrainGenerator::generate(const GenerationContext&ctx,ChunkBuffer&c){for(int x=0;x<16;++x)for(int z=0;z<16;++z){std::uint64_t h=ctx.stage_seed^std::uint64_t((ctx.chunk.x*16+x)*341873128712LL)^std::uint64_t((ctx.chunk.z*16+z)*132897987541LL);h^=h>>33;h*=0xff51afd7ed558ccdULL;int height=sea_+int(h%25)-12;c.fill(x,c.minY(),z,x,height-1,z,stone_);c.setRuntimeId(x,height,z,surface_);}}
}
