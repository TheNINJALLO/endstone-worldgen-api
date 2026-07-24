#include "endstone_worldgen/chunk_buffer.h"
#include <algorithm>
namespace endstone_worldgen {
ChunkBuffer::ChunkBuffer(ChunkPos p,int min_y,int max_y,std::uint32_t fill):pos_(p),min_y_(min_y),max_y_(max_y),blocks_(16*16*(max_y-min_y+1),fill){if(max_y<min_y)throw std::invalid_argument("invalid y range");}
size_t ChunkBuffer::index(int x,int y,int z)const{if(x<0||x>15||z<0||z>15||y<min_y_||y>max_y_)throw std::out_of_range("chunk coordinate");return size_t(y-min_y_)*256+size_t(z)*16+size_t(x);}
std::uint32_t ChunkBuffer::getRuntimeId(int x,int y,int z)const{return blocks_[index(x,y,z)];}
void ChunkBuffer::setRuntimeId(int x,int y,int z,std::uint32_t id){blocks_[index(x,y,z)]=id;}
void ChunkBuffer::fill(int ax,int ay,int az,int bx,int by,int bz,std::uint32_t id){for(int x=std::max(0,std::min(ax,bx));x<=std::min(15,std::max(ax,bx));++x)for(int y=std::max(min_y_,std::min(ay,by));y<=std::min(max_y_,std::max(ay,by));++y)for(int z=std::max(0,std::min(az,bz));z<=std::min(15,std::max(az,bz));++z)setRuntimeId(x,y,z,id);}
void ChunkBuffer::replace(std::uint32_t from,std::uint32_t to){std::replace(blocks_.begin(),blocks_.end(),from,to);}
void ChunkBuffer::setBiome(int x,int y,int z,std::uint32_t b){biomes_[std::uint32_t((y-min_y_)/4*16+(z/4)*4+x/4)]=b;}
void ChunkBuffer::setPaletteEntry(std::uint32_t id, BlockDescriptor descriptor){palette_[id]=std::move(descriptor);}
const BlockDescriptor *ChunkBuffer::paletteEntry(std::uint32_t id)const noexcept{auto it=palette_.find(id);return it==palette_.end()?nullptr:&it->second;}
std::uint64_t ChunkBuffer::fingerprint()const{std::uint64_t h=1469598103934665603ULL;for(auto v:blocks_){h^=v;h*=1099511628211ULL;}return h;}
}
